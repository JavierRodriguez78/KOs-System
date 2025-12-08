#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/e1000/e1000_consts.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>
#include "include/net/nic.hpp"

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::drivers::net::e1000; // bring constants into scope

E1000Driver::E1000Driver() {}

static bool e1000_tx_stub(const kos::common::uint8_t* /*frame*/, kos::common::uint32_t /*len*/) {
    // TX not implemented yet; trace attempts until ring is ready
    Logger::Log("e1000: TX submit called (stub)");
    return false;
}

// Provide a simple RX submission hook the low-level ISR/poll can call
extern "C" void e1000_submit_rx_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    // Forward to NIC-neutral RX path
    kos_nic_driver_rx(frame, len);
}

void E1000Driver::Activate() {
    if (probe_once()) {
        Logger::Log("e1000: device detected (scaffold)");
        log_device();
        // TODO: map MMIO/I/O registers, initialize Rx/Tx rings, register IRQ
        // Attempt to read MAC from RAL/RAH if MMIO base looks valid
        if (io_base_ != 0) {
            auto mmio_read32 = [&](uint32_t reg) -> uint32_t {
                // NOTE: Real implementation should map BAR0 to virtual memory and dereference
                // For now, just return 0 until MMIO is mapped
                (void)reg; return 0u;
            };
            uint32_t ral = mmio_read32(REG_RAL0);
            uint32_t rah = mmio_read32(REG_RAH0);
            if (ral != 0 || rah != 0) {
                uint8_t mac[6];
                mac[0] = (uint8_t)(ral & 0xFF);
                mac[1] = (uint8_t)((ral >> 8) & 0xFF);
                mac[2] = (uint8_t)((ral >> 16) & 0xFF);
                mac[3] = (uint8_t)((ral >> 24) & 0xFF);
                mac[4] = (uint8_t)(rah & 0xFF);
                mac[5] = (uint8_t)((rah >> 8) & 0xFF);
                kos_nic_set_mac(mac);
                Logger::Log("e1000: MAC set via RAL/RAH");
            } else {
                Logger::Log("e1000: MMIO not mapped yet; MAC read deferred");
            }
        }
        // Register NIC-neutral TX so the stack can attempt to send frames
        kos_nic_register_tx(e1000_tx_stub);
    } else {
        Logger::Log("e1000: no device found");
    }
}

bool E1000Driver::probe_once() {
    PeripheralComponentIntercontroller pci;
    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            int functions = pci.DeviceHasFunctions(bus, dev) ? 8 : 1;
            for (int fn = 0; fn < functions; ++fn) {
                auto d = pci.GetDeviceDescriptor(bus, dev, fn);
                if (d.vendor_id == PCI_VENDOR_INTEL && d.device_id == PCI_DEVICE_82540EM) {
                    vendor_ = d.vendor_id; device_ = d.device_id; irq_ = (uint8_t)d.interrupt;
                    uint32_t bar0 = pci.Read(bus, dev, fn, PCI_BAR0_OFFSET);
                    io_base_ = (bar0 & PCI_BAR_ADDR_MASK); // handle I/O vs MMIO elsewhere later
                    found_ = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void E1000Driver::log_device() {
    if (!found_) return;
    Logger::LogKV("e1000.vendor", "0x8086");
    Logger::LogKV("e1000.device", "0x100E");
}
