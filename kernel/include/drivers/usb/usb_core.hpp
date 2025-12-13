#pragma once

#include <common/types.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

namespace kos { namespace drivers { namespace usb {

struct UsbDeviceInfo {
    kos::common::uint8_t address = 0;
    kos::common::uint8_t classCode = 0;
    kos::common::uint8_t subclass = 0;
    kos::common::uint8_t protocol = 0;
};

class UsbCore {
public:
    static bool Init();
    static void Tick(); // polling tick
};

} } }
