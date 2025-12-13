#include <drivers/usb/hid_keyboard.hpp>
#include <drivers/keyboard/keyboard_handler.hpp>

namespace kos { namespace drivers { namespace usb { namespace hid {

static kos::drivers::keyboard::KeyboardEventHandler* s_handler = nullptr;

void HidKeyboard::SetHandler(kos::drivers::keyboard::KeyboardEventHandler* h) { s_handler = h; }

// Very basic boot protocol decode: report[2..7] contain keycodes; ignore modifiers for now
void HidKeyboard::OnReport(const kos::common::uint8_t* report, int len) {
    if (!s_handler || !report || len < 8) return;
    for (int i=2; i<8; ++i) {
        kos::common::uint8_t kc = report[i];
        if (!kc) continue;
        // Map a few common HID keycodes (A-Z, 1-9, 0, Enter, Space, Backspace)
        int8_t ch = 0;
        if (kc >= 0x04 && kc <= 0x1D) { // A-Z
            ch = (int8_t)('a' + (kc - 0x04));
        } else if (kc >= 0x1E && kc <= 0x26) { // 1-9
            ch = (int8_t)('1' + (kc - 0x1E));
        } else if (kc == 0x27) { ch = '0'; }
        else if (kc == 0x28) { ch = '\n'; }
        else if (kc == 0x2C) { ch = ' '; }
        else if (kc == 0x2A) { ch = '\b'; }
        if (ch) s_handler->OnKeyDown(ch);
    }
}

} } } }
