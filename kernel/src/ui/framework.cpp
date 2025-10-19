#include <ui/framework.hpp>
#include <lib/string.hpp>

using namespace kos::ui;
using namespace kos::gfx;

namespace kos { namespace ui {

static UIWindow g_windows[16];
static uint32_t g_count = 0;
static uint32_t g_nextId = 1;

bool Init() {
    g_count = 0; g_nextId = 1;
    return true;
}

uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title) {
    if (g_count >= 16) return 0;
    UIWindow& win = g_windows[g_count++];
    win.id = g_nextId++;
    win.desc.x = x; win.desc.y = y; win.desc.w = w; win.desc.h = h; win.desc.bg = bg; win.desc.title = title;
    return win.id;
}

void RenderAll() {
    for (uint32_t i = 0; i < g_count; ++i) {
        Compositor::DrawWindow(g_windows[i].desc);
    }
}

}} // namespace
