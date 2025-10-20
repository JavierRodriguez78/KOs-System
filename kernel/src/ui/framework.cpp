#include <ui/framework.hpp>
#include <lib/string.hpp>

using namespace kos::ui;
using namespace kos::gfx;

namespace kos { namespace ui {

static UIWindow g_windows[16];
static uint32_t g_count = 0;
static uint32_t g_nextId = 1;
static uint32_t g_zorder[16]; // store indices into g_windows in z order (back..front)

bool Init() {
    g_count = 0; g_nextId = 1;
    for (uint32_t i=0;i<16;++i) g_zorder[i]=i;
    return true;
}

uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title) {
    if (g_count >= 16) return 0;
    UIWindow& win = g_windows[g_count++];
    win.id = g_nextId++;
    win.desc.x = x; win.desc.y = y; win.desc.w = w; win.desc.h = h; win.desc.bg = bg; win.desc.title = title;
    // Put at top of z-order
    // simple: ensure its index is the last in zorder
    for (uint32_t i=0;i<g_count-1;++i) g_zorder[i]=g_zorder[i];
    g_zorder[g_count-1]= (g_count-1);
    return win.id;
}

void RenderAll() {
    for (uint32_t k = 0; k < g_count; ++k) {
        uint32_t idx = g_zorder[k];
        Compositor::DrawWindow(g_windows[idx].desc);
    }
}

static int findIndexById(uint32_t id) {
    for (uint32_t i=0;i<g_count;++i) if (g_windows[i].id == id) return (int)i;
    return -1;
}

bool HitTest(int x, int y, uint32_t& outWindowId, bool& onTitleBar) {
    // Walk z-order from front to back
    for (int32_t k = (int32_t)g_count - 1; k >= 0; --k) {
        uint32_t idx = g_zorder[k];
        const auto& d = g_windows[idx].desc;
        if ((uint32_t)x >= d.x && (uint32_t)x < d.x + d.w && (uint32_t)y >= d.y && (uint32_t)y < d.y + d.h) {
            outWindowId = g_windows[idx].id;
            onTitleBar = ((uint32_t)y < d.y + (d.h < 18 ? d.h : 18));
            return true;
        }
    }
    return false;
}

bool BringToFront(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    // Find in z-order where this index currently is
    int zpos = -1; for (uint32_t k=0;k<g_count;++k) if (g_zorder[k] == (uint32_t)idx) { zpos = (int)k; break; }
    if (zpos < 0) return false;
    // Move to end
    for (uint32_t k = (uint32_t)zpos; k+1 < g_count; ++k) g_zorder[k] = g_zorder[k+1];
    g_zorder[g_count-1] = (uint32_t)idx;
    return true;
}

bool SetWindowPos(uint32_t windowId, uint32_t nx, uint32_t ny) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    g_windows[idx].desc.x = nx; g_windows[idx].desc.y = ny; return true;
}

bool GetWindowDesc(uint32_t windowId, kos::gfx::WindowDesc& outDesc) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    outDesc = g_windows[idx].desc; return true;
}

}} // namespace
