#include <ui/framework.hpp>
#include <ui/input.hpp>
#include <lib/string.hpp>

using namespace kos::ui;
using namespace kos::gfx;

namespace kos { namespace ui {

static UIWindow g_windows[16];
static uint32_t g_count = 0;
static uint32_t g_nextId = 1;
static uint32_t g_zorder[16]; // store indices into g_windows in z order (back..front)
static uint32_t g_focused_window = 0; // window id with keyboard focus
// Interaction state (mouse-driven window ops) moved from WindowManager
static bool g_dragging = false;
static uint32_t g_dragWin = 0;
static int g_dragOffX = 0, g_dragOffY = 0;
static bool g_resizing = false;
static uint32_t g_resizeWin = 0;
static HitRegion g_resizeRegion = HitRegion::None;

// UI event queue (ring buffer)
static UIEvent g_events[64];
static uint32_t g_evt_head = 0; // next pop
static uint32_t g_evt_tail = 0; // next push
static uint32_t g_evt_count = 0;

static void pushEvent(UIEventType t, uint32_t wid, uint32_t x=0, uint32_t y=0) {
    if (g_evt_count >= 64) return; // drop when full (simple policy)
    uint32_t idx = g_evt_tail;
    g_events[idx].type = t;
    g_events[idx].windowId = wid;
    g_events[idx].x = x;
    g_events[idx].y = y;
    g_evt_tail = (g_evt_tail + 1) & 63u;
    ++g_evt_count;
}

bool Init() {
    g_count = 0; g_nextId = 1;
    for (uint32_t i=0;i<16;++i) g_zorder[i]=i;
    g_focused_window = 0;
    return true;
}

uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title, uint32_t flags) {
    if (g_count >= 16) return 0;
    UIWindow& win = g_windows[g_count++];
    win.id = g_nextId++;
    win.desc.x = x; win.desc.y = y; win.desc.w = w; win.desc.h = h; win.desc.bg = bg; win.desc.title = title;
    win.flags = flags;
    win.state = WindowState::Normal;
    win.restore.x = x; win.restore.y = y; win.restore.w = w; win.restore.h = h;
    // Put at top of z-order
    // simple: ensure its index is the last in zorder
    for (uint32_t i=0;i<g_count-1;++i) g_zorder[i]=g_zorder[i];
    g_zorder[g_count-1]= (g_count-1);
    return win.id;
}

void RenderAll() {
    for (uint32_t k = 0; k < g_count; ++k) {
        uint32_t idx = g_zorder[k];
        // Skip minimized windows; the taskbar will represent them
        if (g_windows[idx].state == WindowState::Minimized) continue;
        Compositor::DrawWindow(g_windows[idx].desc);
        // Focus highlight: if focused, overlay a bright accent line under title bar
        if (g_windows[idx].id == g_focused_window) {
            uint32_t x = g_windows[idx].desc.x;
            uint32_t y = g_windows[idx].desc.y + TitleBarHeight(); // bottom of title bar
            uint32_t w = g_windows[idx].desc.w;
            Compositor::FillRect(x, y-2, w, 2, 0xFF3B82F6u); // 2px accent line
        }
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
        uint32_t th = TitleBarHeight();
        uint32_t testH = d.h; // minimized windows are not hit-testable here
        if (g_windows[idx].state == WindowState::Minimized) continue;
        if ((uint32_t)x >= d.x && (uint32_t)x < d.x + d.w && (uint32_t)y >= d.y && (uint32_t)y < d.y + testH) {
            outWindowId = g_windows[idx].id;
            onTitleBar = ((uint32_t)y < d.y + (testH < th ? testH : th));
            return true;
        }
    }
    return false;
}

constexpr uint32_t TitleBarHeight() { return 18; }

void GetStandardButtonRects(const WindowDesc& d, Rect& outMin, Rect& outMax, Rect& outClose) {
    const uint32_t th = TitleBarHeight();
    const uint32_t btnW = 18; // square buttons
    const uint32_t pad = 2;
    // Right to left: Close, Max, Min
    outClose.w = outMax.w = outMin.w = btnW;
    outClose.h = outMax.h = outMin.h = (d.h < th ? d.h : th);
    outClose.x = d.x + d.w - pad - btnW;
    outClose.y = d.y;
    outMax.x = (outClose.x >= btnW+pad ? outClose.x - (btnW+pad) : outClose.x);
    outMax.y = d.y;
    outMin.x = (outMax.x >= btnW+pad ? outMax.x - (btnW+pad) : outMax.x);
    outMin.y = d.y;
}

bool HitTestDetailed(int x, int y, uint32_t& outWindowId, HitRegion& region) {
    region = HitRegion::None;
    uint32_t wid; bool onTitle;
    if (!HitTest(x, y, wid, onTitle)) return false;
    outWindowId = wid;
    int idx = findIndexById(wid);
    if (idx < 0) return false;
    const auto& d = g_windows[idx].desc;
    // Title bar buttons first
    Rect rMin, rMax, rClose; GetStandardButtonRects(d, rMin, rMax, rClose);
    auto inRect=[&](const Rect& r){ return (uint32_t)x >= r.x && (uint32_t)x < r.x + r.w && (uint32_t)y >= r.y && (uint32_t)y < r.y + r.h; };
    if (inRect(rClose) && (g_windows[idx].flags & WF_Closable)) { region = HitRegion::ButtonClose; return true; }
    if (inRect(rMax) && (g_windows[idx].flags & WF_Maximizable)) { region = HitRegion::ButtonMax; return true; }
    if (inRect(rMin) && (g_windows[idx].flags & WF_Minimizable)) { region = HitRegion::ButtonMin; return true; }
    if (onTitle) { region = HitRegion::TitleBar; return true; }
    // Resize edges (simplified): 4px border zones
    const uint32_t bw = 4;
    if (g_windows[idx].flags & WF_Resizable) {
        bool leftEdge = (uint32_t)x >= d.x && (uint32_t)x < d.x + bw;
        bool rightEdge = (uint32_t)x >= d.x + d.w - bw && (uint32_t)x < d.x + d.w;
        bool topEdge = (uint32_t)y >= d.y && (uint32_t)y < d.y + bw;
        bool bottomEdge = (uint32_t)y >= d.y + d.h - bw && (uint32_t)y < d.y + d.h;
        if (leftEdge && topEdge) { region = HitRegion::ResizeTopLeft; return true; }
        if (rightEdge && topEdge) { region = HitRegion::ResizeTopRight; return true; }
        if (leftEdge && bottomEdge) { region = HitRegion::ResizeBottomLeft; return true; }
        if (rightEdge && bottomEdge) { region = HitRegion::ResizeBottomRight; return true; }
        if (leftEdge) { region = HitRegion::ResizeLeft; return true; }
        if (rightEdge) { region = HitRegion::ResizeRight; return true; }
        if (topEdge) { region = HitRegion::ResizeTop; return true; }
        if (bottomEdge) { region = HitRegion::ResizeBottom; return true; }
    }
    region = HitRegion::Client;
    return true;
}

bool BringToFront(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    // Find in z-order where this index currently is
    int zpos = -1; for (uint32_t k=0;k<g_count;++k) if (g_zorder[k] == (uint32_t)idx) { zpos = (int)k; break; }
    if (zpos < 0) return false;
    // Move to end
    for (uint32_t k = (uint32_t)zpos; k+1 < g_count; ++k) g_zorder[k] = g_zorder[k+1];
    g_zorder[g_count-1] = (uint32_t)idx;
    // Optionally set focus when bringing to front (caller can override)
    g_focused_window = windowId;
    pushEvent(UIEventType::WindowFocused, windowId);
    return true;
}

bool SetWindowPos(uint32_t windowId, uint32_t nx, uint32_t ny) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    g_windows[idx].desc.x = nx; g_windows[idx].desc.y = ny; return true;
}

bool SetWindowSize(uint32_t windowId, uint32_t nw, uint32_t nh) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    g_windows[idx].desc.w = nw; g_windows[idx].desc.h = nh; return true;
}

bool GetWindowDesc(uint32_t windowId, kos::gfx::WindowDesc& outDesc) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    outDesc = g_windows[idx].desc; return true;
}

void SetFocusedWindow(uint32_t windowId) {
    // Validate
    int idx = findIndexById(windowId); if (idx < 0) return; g_focused_window = windowId;
}

uint32_t GetFocusedWindow() { return g_focused_window; }

bool CloseWindow(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    // Remove from arrays maintaining order
    // Find z-order position
    int zpos=-1; for (uint32_t k=0;k<g_count;++k) if (g_zorder[k]==(uint32_t)idx){zpos=(int)k;break;}
    if (zpos>=0){ for(uint32_t k=(uint32_t)zpos;k+1<g_count;++k) g_zorder[k]=g_zorder[k+1]; }
    // Shift window array left
    for (uint32_t i=(uint32_t)idx;i+1<g_count;++i) g_windows[i]=g_windows[i+1];
    --g_count;
    // Fix z-order indices referencing shifted windows
    for (uint32_t k=0;k<g_count;++k) if (g_zorder[k]>(uint32_t)idx) --g_zorder[k];
    if (g_focused_window==windowId) g_focused_window=0;
    pushEvent(UIEventType::WindowClosed, windowId);
    return true;
}

bool MinimizeWindow(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    if (!(g_windows[idx].flags & WF_Minimizable)) return false;
    g_windows[idx].state = WindowState::Minimized;
    if (g_focused_window == windowId) g_focused_window = 0;
    pushEvent(UIEventType::WindowMinimized, windowId);
    return true;
}

bool ToggleMaximize(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    if (!(g_windows[idx].flags & WF_Maximizable)) return false;
    auto& w = g_windows[idx];
    if (w.state == WindowState::Maximized) {
        // Restore
        w.desc.x = w.restore.x; w.desc.y = w.restore.y; w.desc.w = w.restore.w; w.desc.h = w.restore.h;
        w.state = WindowState::Normal;
        pushEvent(UIEventType::WindowRestored, windowId);
    } else if (w.state == WindowState::Normal) {
        // Save and maximize to full screen (keeping title bar visible)
        w.restore.x = w.desc.x; w.restore.y = w.desc.y; w.restore.w = w.desc.w; w.restore.h = w.desc.h;
        const auto& fb = kos::gfx::GetInfo();
        w.desc.x = 0; w.desc.y = 0; w.desc.w = fb.width; w.desc.h = fb.height;
        w.state = WindowState::Maximized;
        pushEvent(UIEventType::WindowMaximized, windowId);
    } else if (w.state == WindowState::Minimized) {
        // From minimized to maximized: treat as restore+maximize
        w.state = WindowState::Normal; // intermediate
        return ToggleMaximize(windowId);
    }
    return true;
}

WindowState GetWindowState(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return WindowState::Normal;
    return g_windows[idx].state;
}

uint32_t GetWindowFlags(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return 0;
    return g_windows[idx].flags;
}

bool RestoreWindow(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    if (g_windows[idx].state == WindowState::Minimized) {
        g_windows[idx].state = WindowState::Normal;
        pushEvent(UIEventType::WindowRestored, windowId);
        return true;
    }
    return false;
}

uint32_t GetWindowCount() { return g_count; }

bool GetWindowAt(uint32_t index, uint32_t& outId, WindowDesc& outDesc, WindowState& outState, uint32_t& outFlags) {
    if (index >= g_count) return false;
    uint32_t idx = g_zorder[index]; // return in z-order (back..front)
    outId = g_windows[idx].id;
    outDesc = g_windows[idx].desc;
    outState = g_windows[idx].state;
    outFlags = g_windows[idx].flags;
    return true;
}

// Simplified interaction update (mouse-based). No taskbar logic here yet.
void UpdateInteractions() {
    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    bool left = (mb & 1u) != 0;
    // Begin interaction if not already moving/resizing
    if (!g_dragging && !g_resizing && left) {
        uint32_t wid; HitRegion hr;
        if (HitTestDetailed(mx, my, wid, hr)) {
            BringToFront(wid);
            // Title bar buttons
            if (hr == HitRegion::ButtonClose) {
                CloseWindow(wid); return; // interaction consumed
            } else if (hr == HitRegion::ButtonMin) {
                MinimizeWindow(wid); return;
            } else if (hr == HitRegion::ButtonMax) {
                ToggleMaximize(wid); return;
            } else if (hr == HitRegion::TitleBar) {
                // Restore if minimized
                if (GetWindowState(wid) == WindowState::Minimized) {
                    RestoreWindow(wid);
                }
                WindowDesc d; GetWindowDesc(wid, d);
                g_dragging = true; g_dragWin = wid; g_dragOffX = mx - (int)d.x; g_dragOffY = my - (int)d.y;
            } else {
                // Resizing regions
                switch (hr) {
                    case HitRegion::ResizeLeft:
                    case HitRegion::ResizeRight:
                    case HitRegion::ResizeTop:
                    case HitRegion::ResizeBottom:
                    case HitRegion::ResizeTopLeft:
                    case HitRegion::ResizeTopRight:
                    case HitRegion::ResizeBottomLeft:
                    case HitRegion::ResizeBottomRight:
                        g_resizing = true; g_resizeWin = wid; g_resizeRegion = hr; break;
                    default: break;
                }
            }
        }
    } else if (g_dragging && left) {
        // Dragging move
        WindowDesc d; GetWindowDesc(g_dragWin, d);
        int nx = mx - g_dragOffX; if (nx < 0) nx = 0;
        int ny = my - g_dragOffY; if (ny < 0) ny = 0;
        if ((uint32_t)nx != d.x || (uint32_t)ny != d.y) {
            SetWindowPos(g_dragWin, (uint32_t)nx, (uint32_t)ny);
            pushEvent(UIEventType::WindowMoved, g_dragWin, (uint32_t)nx, (uint32_t)ny);
        }
    } else if (g_resizing && left) {
        WindowDesc d; GetWindowDesc(g_resizeWin, d);
        int x = (int)d.x; int y = (int)d.y; int w = (int)d.w; int h = (int)d.h;
        const int minW = 100, minH = 60;
        auto apply=[&](int nx,int ny,int nw,int nh){ SetWindowPos(g_resizeWin,(uint32_t)nx,(uint32_t)ny); SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)nh); pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)nh); };
        switch (g_resizeRegion) {
            case HitRegion::ResizeLeft: { int nx = mx; int nw = (x + w) - nx; if (nw < minW){ nx = x + w - minW; nw = minW; } apply(nx,y,nw,h); break; }
            case HitRegion::ResizeRight: { int nw = mx - x; if (nw < minW) nw = minW; SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)h); pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)h); break; }
            case HitRegion::ResizeTop: { int ny2 = my; int nh = (y + h) - ny2; if (nh < minH){ ny2 = y + h - minH; nh = minH; } apply(x,ny2,w,nh); break; }
            case HitRegion::ResizeBottom: { int nh = my - y; if (nh < minH) nh = minH; SetWindowSize(g_resizeWin,(uint32_t)w,(uint32_t)nh); pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)w,(uint32_t)nh); break; }
            case HitRegion::ResizeTopLeft: { int nx = mx; int ny2 = my; int nw = (x + w) - nx; int nh = (y + h) - ny2; if (nw < minW){ nx = x + w - minW; nw = minW; } if (nh < minH){ ny2 = y + h - minH; nh = minH; } apply(nx,ny2,nw,nh); break; }
            case HitRegion::ResizeTopRight: { int ny2 = my; int nw = mx - x; int nh = (y + h) - ny2; if (nw < minW) nw = minW; if (nh < minH){ ny2 = y + h - minH; nh = minH; } apply(x,ny2,nw,nh); break; }
            case HitRegion::ResizeBottomLeft: { int nx = mx; int nw = (x + w) - nx; int nh = my - y; if (nw < minW){ nx = x + w - minW; nw = minW; } if (nh < minH) nh = minH; apply(nx,y,nw,nh); break; }
            case HitRegion::ResizeBottomRight: { int nw = mx - x; int nh = my - y; if (nw < minW) nw = minW; if (nh < minH) nh = minH; SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)nh); pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)nh); break; }
            default: break;
        }
    } else if ((g_dragging || g_resizing) && !left) {
        g_dragging = false; g_dragWin = 0;
        g_resizing = false; g_resizeWin = 0; g_resizeRegion = HitRegion::None;
    }
}

bool PollEvent(UIEvent& outEvent) {
    if (g_evt_count == 0) return false;
    uint32_t idx = g_evt_head;
    outEvent = g_events[idx];
    g_evt_head = (g_evt_head + 1) & 63u;
    --g_evt_count;
    return true;
}

}} // namespace
