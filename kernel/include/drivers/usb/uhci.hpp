#pragma once

#include <common/types.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

namespace kos { namespace drivers { namespace usb { namespace uhci {

bool ProbeAndInit();
void Poll();

} } } }
