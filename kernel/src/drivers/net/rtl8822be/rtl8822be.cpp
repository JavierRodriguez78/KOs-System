#include <drivers/net/rtl8822be/rtl8822be.hpp>
#include <console/logger.hpp>
#include <lib/stdio.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;

namespace kos { namespace drivers { namespace net { namespace rtl8822be {

Rtl8822beDriver::Rtl8822beDriver() {}

bool Rtl8822beDriver::probe_once() {
    // Minimal PCI scan to detect RTL8822BE by specific device IDs.
    // Known IDs: 0xB822, 0xB828 (variations/subsystems exist).
    PeripheralComponentIntercontroller pci;
    for (uint8_t bus = 0; bus < 0xFF; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t fn = 0; fn < 8; ++fn) {
                uint32_t id = pci.Read(bus, dev, fn, 0x00);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
                if (vendor == 0x10EC && (device == 0xB822 || device == 0xB828)) {
                    vendor_ = vendor; device_ = device; found_ = true;
                    Logger::LogKV("RTL8822BE: detected at", "bus/dev/fn");
                    // Log raw tuple values as strings
                    char buf[16];
                    kos::sys::snprintf(buf, sizeof(buf), "%u", (unsigned)bus);
                    Logger::LogKV("RTL8822BE: bus", buf);
                    kos::sys::snprintf(buf, sizeof(buf), "%u", (unsigned)dev);
                    Logger::LogKV("RTL8822BE: dev", buf);
                    kos::sys::snprintf(buf, sizeof(buf), "%u", (unsigned)fn);
                    Logger::LogKV("RTL8822BE: fn", buf);
                    return true;
                }
            }
        }
    }
    return false;
}

void Rtl8822beDriver::log_device() {
    if (found_) {
        Logger::LogKV("RTL8822BE: vendor", "0x10EC");
        char buf[16];
        kos::sys::snprintf(buf, sizeof(buf), "0x%04X", (unsigned)device_);
        Logger::LogKV("RTL8822BE: device", buf);
        Logger::Log("RTL8822BE: WiFi device present (stub only)");
    } else {
        Logger::Log("RTL8822BE: device not found (stub)");
    }
}

void Rtl8822beDriver::Activate() {
    Logger::Log("RTL8822BE: Activate (stub WiFi driver)");
    probe_once();
    log_device();
    Logger::Log("RTL8822BE: NOTE: WiFi stack not implemented; interface unavailable");
}

}}}}
