#pragma once

#include <common/types.hpp>
#include <drivers/keyboard/keyboard_handler.hpp>

namespace kos { namespace drivers { namespace usb { namespace hid {

class HidKeyboard {
public:
    static void SetHandler(kos::drivers::keyboard::KeyboardEventHandler* h);
    static void OnReport(const kos::common::uint8_t* report, int len);
};

} } } }
