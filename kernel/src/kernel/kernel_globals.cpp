#include <kernel/globals.hpp>
#include <graphics/framebuffer.hpp>
#include <ui/input.hpp>

// Provide a small MouseEventHandler implementation that forwards to UI when graphics available
class MouseToUI : public kos::drivers::mouse::MouseEventHandler {
public:
    virtual void OnActivate() override {}
    virtual void OnMouseMove(int xoffset, int yoffset) override { if (kos::gfx::IsAvailable()) kos::ui::MouseMove(xoffset, yoffset); }
    virtual void OnMouseDown(uint8_t button) override { if (kos::gfx::IsAvailable()) kos::ui::MouseButtonDown(button); }
    virtual void OnMouseUp(uint8_t button) override { if (kos::gfx::IsAvailable()) kos::ui::MouseButtonUp(button); }
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
// Input diagnostics
kos::common::uint8_t g_kbd_input_source = 0;
kos::common::uint8_t g_mouse_input_source = 0;
kos::common::uint32_t g_kbd_events = 0;
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; ++i)
        (*i)();
}
