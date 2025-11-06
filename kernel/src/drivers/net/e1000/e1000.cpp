#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/e1000/e1000_consts.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::drivers::net::e1000; // bring constants into scope

E1000Driver::E1000Driver() {}

void E1000Driver::Activate() {
    if (probe_once()) {
        Logger::Log("e1000: device detected (scaffold)");
        log_device();
        // TODO: map MMIO/I/O registers, initialize Rx/Tx rings, register IRQ
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
