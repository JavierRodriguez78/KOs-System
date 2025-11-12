#include <drivers/net/rtl8169/rtl8169.hpp>
#include <drivers/net/rtl8169/rtl8169_consts.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::drivers::net::rtl8169;

Rtl8169Driver::Rtl8169Driver() {}

void Rtl8169Driver::Activate() {
    if (probe_once()) {
        Logger::Log("rtl8169: device detected (scaffold)");
        log_device();
        // TODO: Map I/O or MMIO registers, reset chip, enable Rx/Tx, hook IRQ
    } else {
        Logger::Log("rtl8169: no device found");
    }
}

static inline bool is_supported_realtek816x(uint16_t vendor, uint16_t device) {
    if (vendor != PCI_VENDOR_REALTEK) return false;
    return device == PCI_DEVICE_RTL8169
        || device == PCI_DEVICE_RTL8168
        || device == PCI_DEVICE_RTL8101; // treat as supported for now (FE)
}

bool Rtl8169Driver::probe_once() {
    PeripheralComponentIntercontroller pci;
    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            int functions = pci.DeviceHasFunctions(bus, dev) ? 8 : 1;
            for (int fn = 0; fn < functions; ++fn) {
                auto d = pci.GetDeviceDescriptor(bus, dev, fn);
                if (is_supported_realtek816x(d.vendor_id, d.device_id)) {
                    vendor_ = d.vendor_id; device_ = d.device_id; irq_ = (uint8_t)d.interrupt;
                    // BAR0 base I/O port is at config offset 0x10 for many Realtek NICs; MMIO variants also exist
                    uint32_t bar0 = pci.Read(bus, dev, fn, PCI_BAR0_OFFSET);
                    io_base_ = (bar0 & PCI_BAR_IO_MASK);
                    found_ = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void Rtl8169Driver::log_device() {
    if (!found_) return;
    char devStr[7] = { '0','x',0,0,0,0,0 };
    // quick hex formatter for 16-bit
    auto tohex = [](uint16_t v, char* out){
        static const char* h = "0123456789ABCDEF";
        out[0] = '0'; out[1] = 'x';
        out[2] = h[(v >> 12) & 0xF];
        out[3] = h[(v >> 8) & 0xF];
        out[4] = h[(v >> 4) & 0xF];
        out[5] = h[v & 0xF];
        out[6] = 0;
    };
    char ven[7]; char dev[7];
    tohex(vendor_, ven);
    tohex(device_, dev);
    Logger::LogKV("rtl8169.vendor", ven);
    Logger::LogKV("rtl8169.device", dev);
}
