#include <kernel/globals.hpp>
#include <graphics/framebuffer.hpp>
#include <ui/input.hpp>
#include <lib/serial.hpp>

// Provide a small MouseEventHandler implementation that forwards to UI when graphics available
class MouseToUI : public kos::drivers::mouse::MouseEventHandler {
public:
    virtual void OnActivate() override {}
    virtual void OnMouseMove(int xoffset, int yoffset) override {
        if (!kos::gfx::IsAvailable()) return;
        kos::ui::MouseMove(xoffset, yoffset);
        ++kos::g_mouse_ui_moves;
        static uint32_t s_logged_moves = 0;
        if (s_logged_moves < 3) {
            ++s_logged_moves;
            int mx, my;
            uint8_t buttons;
            kos::ui::GetMouseState(mx, my, buttons);
            kos::lib::serial_write("[MOUSE->UI] move x=");
            writeSigned(mx);
            kos::lib::serial_write(" y=");
            writeSigned(my);
            kos::lib::serial_write(" buttons=");
            kos::lib::serial_putc('0' + (buttons & 0x7));
            kos::lib::serial_write("\n");
        }
    }
    virtual void OnMouseDown(uint8_t button) override {
        if (!kos::gfx::IsAvailable()) return;
        kos::ui::MouseButtonDown(button);
        ++kos::g_mouse_ui_button_events;
        logButton("down", button);
    }
    virtual void OnMouseUp(uint8_t button) override {
        if (!kos::gfx::IsAvailable()) return;
        kos::ui::MouseButtonUp(button);
        ++kos::g_mouse_ui_button_events;
        logButton("up", button);
    }

private:
    static void writeSigned(int value) {
        if (value < 0) {
            kos::lib::serial_putc('-');
            value = -value;
        }
        writeUnsigned((uint32_t)value);
    }

    static void writeUnsigned(uint32_t value) {
        if (value == 0) {
            kos::lib::serial_putc('0');
            return;
        }
        char buf[16];
        int len = 0;
        while (value && len < 15) {
            buf[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
        while (len--) kos::lib::serial_putc(buf[len]);
    }

    static void logButton(const char* phase, uint8_t button) {
        static uint32_t s_logged_buttons = 0;
        if (s_logged_buttons >= 6) return;
        ++s_logged_buttons;
        int mx, my;
        uint8_t buttons;
        kos::ui::GetMouseState(mx, my, buttons);
        kos::lib::serial_write("[MOUSE->UI] button ");
        kos::lib::serial_write(phase);
        kos::lib::serial_write(" b=");
        kos::lib::serial_putc('0' + (button % 10));
        kos::lib::serial_write(" x=");
        writeSigned(mx);
        kos::lib::serial_write(" y=");
        writeSigned(my);
        kos::lib::serial_write(" mask=");
        kos::lib::serial_putc('0' + (buttons & 0x7));
        kos::lib::serial_write("\n");
    }
};

static MouseToUI s_mouse_ui_handler;


namespace kos {
kos::drivers::mouse::MouseDriver* g_mouse_driver_ptr = nullptr;
kos::drivers::mouse::MouseEventHandler* g_mouse_ui_handler_ptr = &s_mouse_ui_handler;
kos::console::Shell* g_shell = nullptr;
kos::console::Shell g_shell_instance;
kos::common::uint8_t g_mouse_poll_mode = 2; // default poll mode
kos::kernel::DisplayMode g_display_mode = kos::kernel::DisplayMode::Graphics; // default display mode
drivers::keyboard::KeyboardDriver* g_keyboard_driver_ptr = nullptr; // set in InitDrivers
// Global keyboard handler override - when non-null, all keyboard events go here
drivers::keyboard::KeyboardEventHandler* g_keyboard_handler_override = nullptr;
// Input diagnostics
kos::common::uint8_t g_kbd_input_source = 0;
kos::common::uint8_t g_mouse_input_source = 0;
kos::common::uint32_t g_kbd_events = 0;
kos::common::uint32_t g_mouse_ui_moves = 0;
kos::common::uint32_t g_mouse_ui_button_events = 0;
bool g_kbd_poll_enabled = false; // Disabled until keyboard init complete
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; ++i)
        (*i)();
}
