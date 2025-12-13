#include <drivers/usb/usb_core.hpp>
#include <drivers/usb/uhci.hpp>
#include <console/logger.hpp>

using kos::console::Logger;

namespace kos { namespace drivers { namespace usb {

bool UsbCore::Init() {
    bool ok = uhci::ProbeAndInit();
    if (ok) Logger::Log("USB: UHCI initialized");
    else Logger::Log("USB: UHCI not found");
    return ok;
}

void UsbCore::Tick() {
    uhci::Poll();
}

} } }
