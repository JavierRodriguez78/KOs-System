#include <ui/cursor.hpp>
#include <ui/input.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>

namespace kos { namespace ui {

static CursorStyle g_style = CursorStyle::Triangle;

void SetCursorStyle(CursorStyle s) { g_style = s; }
CursorStyle GetCursorStyle() { return g_style; }

void RenderCursor() {
    if (!kos::gfx::IsAvailable()) return;
    int mx, my; uint8_t mb; GetMouseState(mx, my, mb);
    const auto& fb = kos::gfx::GetInfo();
    if (mx < 0) mx = 0; if (my < 0) my = 0;
    if (mx >= (int)fb.width) mx = (int)fb.width - 1;
    if (my >= (int)fb.height) my = (int)fb.height - 1;

    // Determine fill color based on button state while keeping dark border for readability.
    uint32_t fill = 0xFFFFFFFFu; // white default
    if (mb & 1u) fill = 0xFFFFE3E3u;      // left pressed
    else if (mb & 4u) fill = 0xFFE3FFE3u; // middle pressed
    else if (mb & 2u) fill = 0xFFE3E3FFu; // right pressed

    switch (g_style) {
        case CursorStyle::Crosshair: {
            // Crosshair 7x7 centered at (mx,my)
            for (int dx=-3; dx<=3; ++dx) {
                int x = mx + dx; int y = my; if (x>=0 && x<(int)fb.width)
                    kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dx==0? fill : 0xFFFFFFFFu));
            }
            for (int dy=-3; dy<=3; ++dy) {
                int x = mx; int y = my + dy; if (y>=0 && y<(int)fb.height)
                    kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dy==0? fill : 0xFFFFFFFFu));
            }
            break; }
        case CursorStyle::Triangle: {
            // Classic arrow cursor (hotspot at mx,my): black outline + white interior.
            // 0 = transparent, 1 = border, 2 = fill.
            static const uint8_t shape[18][12] = {
                {1,0,0,0,0,0,0,0,0,0,0,0},
                {1,1,0,0,0,0,0,0,0,0,0,0},
                {1,2,1,0,0,0,0,0,0,0,0,0},
                {1,2,2,1,0,0,0,0,0,0,0,0},
                {1,2,2,2,1,0,0,0,0,0,0,0},
                {1,2,2,2,2,1,0,0,0,0,0,0},
                {1,2,2,2,2,2,1,0,0,0,0,0},
                {1,2,2,2,2,2,2,1,0,0,0,0},
                {1,2,2,2,2,2,2,2,1,0,0,0},
                {1,2,2,2,2,2,2,2,2,1,0,0},
                {1,2,2,2,2,2,2,1,1,1,0,0},
                {1,2,2,1,2,2,2,1,0,0,0,0},
                {1,2,1,0,1,2,2,2,1,0,0,0},
                {1,1,0,0,1,2,2,2,1,0,0,0},
                {0,0,0,0,0,1,2,2,2,1,0,0},
                {0,0,0,0,0,1,2,2,2,1,0,0},
                {0,0,0,0,0,0,1,2,2,1,0,0},
                {0,0,0,0,0,0,1,1,1,0,0,0}
            };

            for (int j = 0; j < 18; ++j) {
                for (int i = 0; i < 12; ++i) {
                    uint8_t px = shape[j][i];
                    if (px == 0) continue;
                    int x = mx + i;
                    int y = my + j;
                    if (x < 0 || y < 0 || x >= (int)fb.width || y >= (int)fb.height) continue;
                    uint32_t color = (px == 1) ? 0xFF000000u : fill;
                    kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, color);
                }
            }
            break; }
    }
}

}} // namespace
