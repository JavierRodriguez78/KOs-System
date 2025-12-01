#include <kernel/mouse_handlers.hpp>
#include <kernel/globals.hpp>
#include <drivers/mouse/mouse_event_handler.hpp>
#include <console/tty.hpp>

using namespace kos;
using namespace kos::drivers::mouse;

namespace kos { namespace kernel {

// Mouse handler that draws a simple cursor on the text console. Kept
// minimal and using exact signatures from MouseEventHandler so it can be
// extracted into its own compilation unit.
class MouseToConsole : public kos::drivers::mouse::MouseEventHandler {
    int32_t x, y;
public:
    MouseToConsole() : x(40), y(12) {}
    virtual void OnActivate() override {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40; y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    virtual void OnMouseMove(int32_t xoffset, int32_t yoffset) override {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if (x >= 80) x = 79;
        if (x < 0) x = 0;
        y += yoffset;
        if (y >= 25) y = 24;
        if (y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    virtual void OnMouseDown(uint8_t) override {}
    virtual void OnMouseUp(uint8_t) override {}
};

// Keep the instance internal to this module and ensure construction
// happens from `InitKernelMouseHandlers()` which kernelMain will call
// before `InitDrivers()`.
static MouseToConsole s_mouse_console_handler;

void InitKernelMouseHandlers()
{
    // Ensure the console handler is constructed and, if you want the
    // console handler to be the active UI handler in non-graphical mode,
    // you could set `g_mouse_ui_handler_ptr` here. Currently the UI handler
    // (for graphics) is defined in `kernel_globals.cpp` and assigned there.
    (void)&s_mouse_console_handler; // touch to avoid optimiser warnings
}

} }
