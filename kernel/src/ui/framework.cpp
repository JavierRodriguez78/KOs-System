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
static Rect g_work_area = {0, 0, 0, 0};
static bool g_has_work_area = false;
static uint32_t g_auto_place_counter = 0;
static bool g_user_focus_interaction = false;

static int findIndexById(uint32_t id);

static uint8_t zRoleRank(WindowRole role) {
    // Higher rank = more on-top preference.
    switch (role) {
        case WindowRole::Dock: return 3;
        case WindowRole::Dialog: return 2;
        case WindowRole::Utility: return 1;
        case WindowRole::Normal:
        default: return 0;
    }
}

static void moveWindowToLayerTop(uint32_t idx) {
    int cur = -1;
    for (uint32_t i = 0; i < g_count; ++i) {
        if (g_zorder[i] == idx) { cur = (int)i; break; }
    }
    if (cur < 0) return;

    uint8_t rank = zRoleRank(g_windows[idx].role);
    int target = cur;
    // Move forward while next window has rank <= this rank.
    // This keeps higher-ranked windows always above lower-ranked ones.
    while ((uint32_t)(target + 1) < g_count) {
        uint32_t nextIdx = g_zorder[target + 1];
        uint8_t nextRank = zRoleRank(g_windows[nextIdx].role);
        if (nextRank > rank) break;
        ++target;
    }

    if (target == cur) return;
    uint32_t v = g_zorder[cur];
    for (int i = cur; i < target; ++i) g_zorder[i] = g_zorder[i + 1];
    g_zorder[target] = v;
}

static Rect effectiveWorkArea() {
    if (g_has_work_area && g_work_area.w > 0 && g_work_area.h > 0) return g_work_area;
    const auto& fb = kos::gfx::GetInfo();
    Rect r;
    r.x = 0; r.y = 0; r.w = fb.width; r.h = fb.height;
    return r;
}

static void clampWindowToArea(WindowDesc& d) {
    Rect a = effectiveWorkArea();
    if (a.w == 0 || a.h == 0) return;

    const uint32_t minW = 100;
    const uint32_t minH = 60;

    if (d.w < minW) d.w = minW;
    if (d.h < minH) d.h = minH;
    if (d.w > a.w) d.w = a.w;
    if (d.h > a.h) d.h = a.h;

    if (d.x < a.x) d.x = a.x;
    if (d.y < a.y) d.y = a.y;

    if (d.x + d.w > a.x + a.w) d.x = (a.x + a.w) - d.w;
    if (d.y + d.h > a.y + a.h) d.y = (a.y + a.h) - d.h;
}

static uint32_t rectOverlapArea(const WindowDesc& a, const WindowDesc& b) {
    uint32_t ax2 = a.x + a.w;
    uint32_t ay2 = a.y + a.h;
    uint32_t bx2 = b.x + b.w;
    uint32_t by2 = b.y + b.h;

    uint32_t ix1 = (a.x > b.x) ? a.x : b.x;
    uint32_t iy1 = (a.y > b.y) ? a.y : b.y;
    uint32_t ix2 = (ax2 < bx2) ? ax2 : bx2;
    uint32_t iy2 = (ay2 < by2) ? ay2 : by2;

    if (ix2 <= ix1 || iy2 <= iy1) return 0;
    return (ix2 - ix1) * (iy2 - iy1);
}

static uint64_t totalOverlapAreaForCandidate(const WindowDesc& cand) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < g_count; ++i) {
        if (g_windows[i].state == WindowState::Minimized) continue;
        total += (uint64_t)rectOverlapArea(cand, g_windows[i].desc);
    }
    return total;
}

static uint32_t absDiffU32(uint32_t a, uint32_t b) { return (a > b) ? (a - b) : (b - a); }

static void placeWindowMinOverlap(WindowDesc& d, uint32_t prefX, uint32_t prefY) {
    Rect a = effectiveWorkArea();
    if (a.w == 0 || a.h == 0) return;
    if (d.w > a.w || d.h > a.h) return;

    uint32_t maxX = a.x + a.w - d.w;
    uint32_t maxY = a.y + a.h - d.h;

    // Keep preferred point inside bounds.
    if (prefX < a.x) prefX = a.x;
    if (prefY < a.y) prefY = a.y;
    if (prefX > maxX) prefX = maxX;
    if (prefY > maxY) prefY = maxY;

    uint32_t bestX = prefX;
    uint32_t bestY = prefY;
    uint64_t bestOverlap = ~0ull;
    uint32_t bestDist = 0xFFFFFFFFu;

    const uint32_t step = 24;
    for (uint32_t y = a.y; y <= maxY; y = (y + step <= maxY ? y + step : maxY + 1)) {
        for (uint32_t x = a.x; x <= maxX; x = (x + step <= maxX ? x + step : maxX + 1)) {
            WindowDesc c = d;
            c.x = x;
            c.y = y;
            uint64_t overlap = totalOverlapAreaForCandidate(c);
            uint32_t dist = absDiffU32(x, prefX) + absDiffU32(y, prefY);

            if (overlap < bestOverlap || (overlap == bestOverlap && dist < bestDist)) {
                bestOverlap = overlap;
                bestDist = dist;
                bestX = x;
                bestY = y;
                if (bestOverlap == 0 && bestDist == 0) {
                    d.x = bestX;
                    d.y = bestY;
                    return;
                }
            }
        }
    }

    // Also evaluate preferred exact coordinate (in case it is off-grid and better).
    {
        WindowDesc c = d;
        c.x = prefX;
        c.y = prefY;
        uint64_t overlap = totalOverlapAreaForCandidate(c);
        uint32_t dist = 0;
        if (overlap < bestOverlap || (overlap == bestOverlap && dist < bestDist)) {
            bestX = prefX;
            bestY = prefY;
        }
    }

    d.x = bestX;
    d.y = bestY;
}

static void autoPlaceWindow(WindowDesc& d, WindowRole role, uint32_t parentId) {
    Rect a = effectiveWorkArea();
    if (a.w == 0 || a.h == 0) return;

    if (role == WindowRole::Dialog) {
        int pidx = (parentId != 0) ? findIndexById(parentId) : -1;
        if (pidx >= 0) {
            const WindowDesc& p = g_windows[pidx].desc;
            // Transient dialog: center over parent window.
            d.x = p.x + ((p.w > d.w) ? ((p.w - d.w) / 2) : 0);
            d.y = p.y + ((p.h > d.h) ? ((p.h - d.h) / 2) : 0);
        } else {
            // Dialog without parent: center in work area.
            d.x = a.x + ((a.w > d.w) ? ((a.w - d.w) / 2) : 0);
            d.y = a.y + ((a.h > d.h) ? ((a.h - d.h) / 2) : 0);
        }
    } else if (role == WindowRole::Utility) {
        // Utility windows tend to live near edges; top-right by default.
        uint32_t pad = 8;
        uint32_t rx = (a.w > d.w + pad) ? (a.x + a.w - d.w - pad) : a.x;
        uint32_t ry = (a.h > d.h + pad) ? (a.y + pad) : a.y;
        d.x = rx;
        d.y = ry;
    } else if (role == WindowRole::Dock) {
        // Dock reserves edge-aligned geometry; current default is bottom strip.
        d.x = a.x;
        d.w = a.w;
        d.y = (a.h > d.h) ? (a.y + a.h - d.h) : a.y;
    } else {
        // First normal window centered; subsequent windows cascade.
        if (g_auto_place_counter == 0) {
            d.x = a.x + ((a.w > d.w) ? ((a.w - d.w) / 2) : 0);
            d.y = a.y + ((a.h > d.h) ? ((a.h - d.h) / 3) : 0);
        } else {
            const uint32_t dx = 28;
            const uint32_t dy = 20;
            const uint32_t spanX = (a.w > d.w) ? (a.w - d.w) : 0;
            const uint32_t spanY = (a.h > d.h) ? (a.h - d.h) : 0;
            const uint32_t offX = (spanX == 0) ? 0 : ((g_auto_place_counter * dx) % (spanX + 1));
            const uint32_t offY = (spanY == 0) ? 0 : ((g_auto_place_counter * dy) % (spanY + 1));
            d.x = a.x + offX;
            d.y = a.y + offY;
        }

        // Reduce overlap among normal windows by searching near preferred spot.
        placeWindowMinOverlap(d, d.x, d.y);
    }

    ++g_auto_place_counter;
    clampWindowToArea(d);
}

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
    g_auto_place_counter = 0;
    g_has_work_area = false;
    return true;
}

void SetWorkArea(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) {
        ResetWorkArea();
        return;
    }
    g_work_area.x = x;
    g_work_area.y = y;
    g_work_area.w = w;
    g_work_area.h = h;
    g_has_work_area = true;
}

void ResetWorkArea() {
    g_has_work_area = false;
}

bool GetWorkArea(Rect& outArea) {
    outArea = effectiveWorkArea();
    return true;
}

uint32_t CreateWindowEx(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title,
                        WindowRole role, uint32_t flags, uint32_t parentWindowId) {
    if (g_count >= 16) return 0;

    if (parentWindowId && findIndexById(parentWindowId) < 0) parentWindowId = 0;

    WindowDesc d;
    d.x = x;
    d.y = y;
    d.w = w;
    d.h = h;
    d.bg = bg;
    d.title = title;

    // kAutoCoord delegates placement to the framework.
    if (x == kAutoCoord || y == kAutoCoord) {
        autoPlaceWindow(d, role, parentWindowId);
    } else {
        clampWindowToArea(d);
    }

    UIWindow& win = g_windows[g_count++];
    win.id = g_nextId++;
    win.desc = d;
    win.flags = flags;
    win.state = WindowState::Normal;
    win.role = role;
    win.parentId = parentWindowId;
    win.restore.x = d.x; win.restore.y = d.y; win.restore.w = d.w; win.restore.h = d.h;
    // Put at top of z-order
    // simple: ensure its index is the last in zorder
    for (uint32_t i=0;i<g_count-1;++i) g_zorder[i]=g_zorder[i];
    g_zorder[g_count-1]= (g_count-1);
    // Respect role-based layering (dock/dialog/utility above normal windows).
    moveWindowToLayerTop((uint32_t)(g_count - 1));
    return win.id;
}

uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title, uint32_t flags) {
    return CreateWindowEx(x, y, w, h, bg, title, WindowRole::Normal, flags, 0);
}

uint32_t CreateDialogWindow(uint32_t parentWindowId,
                            uint32_t w, uint32_t h, uint32_t bg, const char* title,
                            uint32_t flags) {
    return CreateWindowEx(kAutoCoord, kAutoCoord, w, h, bg, title, WindowRole::Dialog, flags, parentWindowId);
}

void RenderAll() {
    for (uint32_t k = 0; k < g_count; ++k) {
        uint32_t idx = g_zorder[k];
        // Skip minimized windows; the taskbar will represent them
        if (g_windows[idx].state == WindowState::Minimized) continue;
        if (g_windows[idx].flags & WF_Frameless) {
            const auto& d = g_windows[idx].desc;
            Compositor::FillRect(d.x, d.y, d.w, d.h, d.bg);
            Compositor::FillRect(d.x, d.y, d.w, 1, 0x66FFFFFFu);
            Compositor::FillRect(d.x, d.y + d.h - 1, d.w, 1, 0x66000000u);
            Compositor::FillRect(d.x, d.y, 1, d.h, 0x66FFFFFFu);
            Compositor::FillRect(d.x + d.w - 1, d.y, 1, d.h, 0x66000000u);
        } else {
            Compositor::DrawWindow(g_windows[idx].desc);
        }
        // Focus highlight: if focused, overlay a bright accent line under title bar
        if (g_windows[idx].id == g_focused_window && !(g_windows[idx].flags & WF_Frameless)) {
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
        uint32_t th = (g_windows[idx].flags & WF_Frameless) ? 0 : TitleBarHeight();
        uint32_t testH = d.h; // minimized windows are not hit-testable here
        if (g_windows[idx].state == WindowState::Minimized) continue;
        if ((uint32_t)x >= d.x && (uint32_t)x < d.x + d.w && (uint32_t)y >= d.y && (uint32_t)y < d.y + testH) {
            outWindowId = g_windows[idx].id;
            onTitleBar = (th != 0) && ((uint32_t)y < d.y + (testH < th ? testH : th));
            return true;
        }
    }
    return false;
}

// TitleBarHeight is defined inline in the header.

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
    if (g_windows[idx].flags & WF_Frameless) {
        region = HitRegion::Client;
        return true;
    }
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
    // Move to top but constrained by role layer policy.
    moveWindowToLayerTop((uint32_t)idx);
    // Focus stealing prevention: programmatic raises do not steal focus unless
    // this raise comes from direct user interaction.
    if (g_user_focus_interaction || g_focused_window == 0 || g_focused_window == windowId) {
        g_focused_window = windowId;
        pushEvent(UIEventType::WindowFocused, windowId);
    }
    return true;
}

bool SetWindowPos(uint32_t windowId, uint32_t nx, uint32_t ny) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    g_windows[idx].desc.x = nx;
    g_windows[idx].desc.y = ny;
    clampWindowToArea(g_windows[idx].desc);
    return true;
}

bool SetWindowSize(uint32_t windowId, uint32_t nw, uint32_t nh) {
    int idx = findIndexById(windowId); if (idx < 0) return false;
    g_windows[idx].desc.w = nw;
    g_windows[idx].desc.h = nh;
    clampWindowToArea(g_windows[idx].desc);
    return true;
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

    // Close transient children first so no orphan dialogs remain.
    while (true) {
        uint32_t childId = 0;
        for (uint32_t i = 0; i < g_count; ++i) {
            if (g_windows[i].parentId == windowId) {
                childId = g_windows[i].id;
                break;
            }
        }
        if (childId == 0) break;
        if (childId == windowId) break;
        CloseWindow(childId);
        idx = findIndexById(windowId);
        if (idx < 0) return true;
    }

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
        clampWindowToArea(w.desc);
        w.state = WindowState::Normal;
        pushEvent(UIEventType::WindowRestored, windowId);
    } else if (w.state == WindowState::Normal) {
        // Save and maximize to full screen (keeping title bar visible)
        w.restore.x = w.desc.x; w.restore.y = w.desc.y; w.restore.w = w.desc.w; w.restore.h = w.desc.h;
        Rect a = effectiveWorkArea();
        w.desc.x = a.x; w.desc.y = a.y; w.desc.w = a.w; w.desc.h = a.h;
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
            g_user_focus_interaction = true;
            BringToFront(wid);
            g_user_focus_interaction = false;
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
        Rect a = effectiveWorkArea();
        int nx = mx - g_dragOffX;
        int ny = my - g_dragOffY;
        int minX = (int)a.x;
        int minY = (int)a.y;
        int maxX = (int)(a.x + a.w - d.w);
        int maxY = (int)(a.y + a.h - d.h);
        if (nx < minX) nx = minX;
        if (ny < minY) ny = minY;
        if (nx > maxX) nx = maxX;
        if (ny > maxY) ny = maxY;
        if ((uint32_t)nx != d.x || (uint32_t)ny != d.y) {
            SetWindowPos(g_dragWin, (uint32_t)nx, (uint32_t)ny);
            pushEvent(UIEventType::WindowMoved, g_dragWin, (uint32_t)nx, (uint32_t)ny);
        }
    } else if (g_resizing && left) {
        WindowDesc d; GetWindowDesc(g_resizeWin, d);
        Rect a = effectiveWorkArea();
        int x = (int)d.x; int y = (int)d.y; int w = (int)d.w; int h = (int)d.h;
        const int minW = 100, minH = 60;
        auto apply=[&](int nx,int ny,int nw,int nh){ SetWindowPos(g_resizeWin,(uint32_t)nx,(uint32_t)ny); SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)nh); pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)nh); };
        switch (g_resizeRegion) {
            case HitRegion::ResizeLeft: {
                int nx = mx;
                if (nx < (int)a.x) nx = (int)a.x;
                int nw = (x + w) - nx;
                if (nw < minW) { nx = x + w - minW; nw = minW; }
                apply(nx,y,nw,h);
                break;
            }
            case HitRegion::ResizeRight: {
                int nw = mx - x;
                if (nw < minW) nw = minW;
                if (x + nw > (int)(a.x + a.w)) nw = (int)(a.x + a.w) - x;
                SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)h);
                pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)h);
                break;
            }
            case HitRegion::ResizeTop: {
                int ny2 = my;
                if (ny2 < (int)a.y) ny2 = (int)a.y;
                int nh = (y + h) - ny2;
                if (nh < minH){ ny2 = y + h - minH; nh = minH; }
                apply(x,ny2,w,nh);
                break;
            }
            case HitRegion::ResizeBottom: {
                int nh = my - y;
                if (nh < minH) nh = minH;
                if (y + nh > (int)(a.y + a.h)) nh = (int)(a.y + a.h) - y;
                SetWindowSize(g_resizeWin,(uint32_t)w,(uint32_t)nh);
                pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)w,(uint32_t)nh);
                break;
            }
            case HitRegion::ResizeTopLeft: {
                int nx = mx; int ny2 = my;
                if (nx < (int)a.x) nx = (int)a.x;
                if (ny2 < (int)a.y) ny2 = (int)a.y;
                int nw = (x + w) - nx;
                int nh = (y + h) - ny2;
                if (nw < minW){ nx = x + w - minW; nw = minW; }
                if (nh < minH){ ny2 = y + h - minH; nh = minH; }
                apply(nx,ny2,nw,nh);
                break;
            }
            case HitRegion::ResizeTopRight: {
                int ny2 = my;
                if (ny2 < (int)a.y) ny2 = (int)a.y;
                int nw = mx - x;
                int nh = (y + h) - ny2;
                if (nw < minW) nw = minW;
                if (x + nw > (int)(a.x + a.w)) nw = (int)(a.x + a.w) - x;
                if (nh < minH){ ny2 = y + h - minH; nh = minH; }
                apply(x,ny2,nw,nh);
                break;
            }
            case HitRegion::ResizeBottomLeft: {
                int nx = mx;
                if (nx < (int)a.x) nx = (int)a.x;
                int nw = (x + w) - nx;
                int nh = my - y;
                if (nw < minW){ nx = x + w - minW; nw = minW; }
                if (nh < minH) nh = minH;
                if (y + nh > (int)(a.y + a.h)) nh = (int)(a.y + a.h) - y;
                apply(nx,y,nw,nh);
                break;
            }
            case HitRegion::ResizeBottomRight: {
                int nw = mx - x;
                int nh = my - y;
                if (nw < minW) nw = minW;
                if (nh < minH) nh = minH;
                if (x + nw > (int)(a.x + a.w)) nw = (int)(a.x + a.w) - x;
                if (y + nh > (int)(a.y + a.h)) nh = (int)(a.y + a.h) - y;
                SetWindowSize(g_resizeWin,(uint32_t)nw,(uint32_t)nh);
                pushEvent(UIEventType::WindowResized,g_resizeWin,(uint32_t)nw,(uint32_t)nh);
                break;
            }
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

WindowRole GetWindowRole(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return WindowRole::Normal;
    return g_windows[idx].role;
}

uint32_t GetWindowParent(uint32_t windowId) {
    int idx = findIndexById(windowId); if (idx < 0) return 0;
    return g_windows[idx].parentId;
}

}} // namespace
