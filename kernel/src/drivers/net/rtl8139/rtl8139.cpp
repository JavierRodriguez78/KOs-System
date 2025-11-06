#include <drivers/net/rtl8139/rtl8139.hpp>
#include <drivers/net/rtl8139/rtl8139_consts.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::drivers::net::rtl8139;


Rtl8139Driver::Rtl8139Driver() {}

void Rtl8139Driver::Activate() {
    if (probe_once()) {
        Logger::Log("rtl8139: device detected (scaffold)");
        log_device();
        // TODO: map I/O registers, reset chip, enable Rx/Tx, register IRQ handler
    } else {
        Logger::Log("rtl8139: no device found");
    }
}

bool Rtl8139Driver::probe_once() {
    PeripheralComponentIntercontroller pci;
    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            int functions = pci.DeviceHasFunctions(bus, dev) ? 8 : 1;
            for (int fn = 0; fn < functions; ++fn) {
                auto d = pci.GetDeviceDescriptor(bus, dev, fn);
                if (d.vendor_id == PCI_VENDOR_REALTEK && d.device_id == PCI_DEVICE_RTL8139) {
                    vendor_ = d.vendor_id; device_ = d.device_id; irq_ = (uint8_t)d.interrupt;
                    // BAR0 base I/O port is at config offset 0x10
                    uint32_t bar0 = pci.Read(bus, dev, fn, PCI_BAR0_OFFSET);
                    io_base_ = (bar0 & PCI_BAR_IO_MASK); // I/O space: lower bits indicate type
                    found_ = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void Rtl8139Driver::log_device() {
    if (!found_) return;
    Logger::LogKV("rtl8139.vendor", "0x10EC");
    Logger::LogKV("rtl8139.device", "0x8139");
    Logger::LogKV("rtl8139.iobase", "(see debug)");
}
