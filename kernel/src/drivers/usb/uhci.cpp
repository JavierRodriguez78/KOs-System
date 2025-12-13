#include <drivers/usb/uhci.hpp>
#include <drivers/usb/hid_keyboard.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <kernel/globals.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>

using kos::console::Logger;
using namespace kos::arch::x86::hardware::pci;

namespace kos { namespace drivers { namespace usb { namespace uhci {

static bool s_inited = false;

bool ProbeAndInit() {
    PeripheralComponentIntercontroller pci;
    // Brute-force scan PCI buses/devices/functions
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint16_t devn = 0; devn < 32; ++devn) {
            bool multi = pci.DeviceHasFunctions(bus, devn);
            uint16_t fnMax = multi ? 8 : 1;
            for (uint16_t fn = 0; fn < fnMax; ++fn) {
                auto dev = pci.GetDeviceDescriptor(bus, devn, fn);
                // Skip empty function slots
                if (dev.vendor_id == 0xFFFF) continue;
                if (dev.class_id == 0x0C && dev.subclass_id == 0x03 && dev.interface_id == 0x00) {
                    Logger::Log("UHCI: controller detected");
                    s_inited = true;
                    // TODO: Read BARs, setup frame list/QH/TDs, enable ports.
                    return true;
                }
            }
        }
    }
    return false;
}

void Poll() {
    if (!s_inited) return;
    // Minimal stub: in a full impl, poll interrupt endpoint ring for HID reports.
    // For now, do nothing.
}

} } } }
