#include <ui/cursor.hpp>
#include <ui/input.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>

namespace kos { namespace ui {

static CursorStyle g_style = CursorStyle::Crosshair;

void SetCursorStyle(CursorStyle s) { g_style = s; }
CursorStyle GetCursorStyle() { return g_style; }

void RenderCursor() {
    if (!kos::gfx::IsAvailable()) return;
    int mx, my; uint8_t mb; GetMouseState(mx, my, mb);
    const auto& fb = kos::gfx::GetInfo();
    if (mx < 0) mx = 0; if (my < 0) my = 0;
    if (mx >= (int)fb.width) mx = (int)fb.width - 1;
    if (my >= (int)fb.height) my = (int)fb.height - 1;

    // Determine primary color based on button state (left/middle/right)
    uint32_t base = 0xFFFFFFFFu; // white default
    if (mb & 1u) base = 0xFFFF4040u; // left pressed
    else if (mb & 4u) base = 0xFF40FF40u; // middle pressed
    else if (mb & 2u) base = 0xFF4040FFu; // right pressed

    switch (g_style) {
        case CursorStyle::Crosshair: {
            // Crosshair 7x7 centered at (mx,my)
            for (int dx=-3; dx<=3; ++dx) {
                int x = mx + dx; int y = my; if (x>=0 && x<(int)fb.width)
                    kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dx==0? base : 0xFFFFFFFFu));
            }
            for (int dy=-3; dy<=3; ++dy) {
                int x = mx; int y = my + dy; if (y>=0 && y<(int)fb.height)
                    kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dy==0? base : 0xFFFFFFFFu));
            }
            break; }
        case CursorStyle::Triangle: {
            // Simple 10x16 left-facing triangle (like classic arrow) with top at (mx,my)
            for (int j = 0; j < 16; ++j) {
                int rowW = (j < 10 ? j+1 : 10);
                for (int i = 0; i < rowW; ++i) {
                    int x = mx + i; int y = my + j;
                    if (x >=0 && y>=0 && x < (int)fb.width && y < (int)fb.height) {
                        kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, base);
                    }
                }
            }
            break; }
    }
}

}} // namespace
