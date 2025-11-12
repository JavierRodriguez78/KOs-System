#include <services/window_manager.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <ui/framework.hpp>
#include <ui/input.hpp>
#include <graphics/terminal.hpp>
#include <graphics/font8x8_basic.hpp>
#include <services/service_manager.hpp>
#include <console/tty.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/mouse/mouse_driver.hpp>

// Global mouse driver pointer provided by kernel.cpp (defined there)
extern kos::drivers::mouse::MouseDriver* g_mouse_driver_ptr;

using namespace kos::services;

namespace kos { namespace services {

static uint32_t t = 0;
static uint32_t s_mouse_win_id = 0; // small diagnostics window for mouse position
// 0=never, 1=until first packet, 2=always (default)
static uint8_t s_mouse_poll_mode = 2;
static bool s_taskbar_enabled = true;
static uint32_t s_terminal_counter = 1; // for titling Terminal N

// Special taskbar hit ids
static constexpr uint32_t kTaskbarSpawnBtnId = 0xFFFFFFFFu; // sentinel for "+" button

// Taskbar layout constants
static constexpr uint32_t kTaskbarHeight = 22; // bottom panel height
static constexpr uint32_t kTaskbarPad = 4;
static constexpr uint32_t kTaskButtonW = 120; // fixed width per window button
static constexpr uint32_t kTaskButtonH = 18;  // inside bar

struct TaskbarButtonGeom { uint32_t x,y,w,h; uint32_t winId; bool minimized; bool focused; };
static TaskbarButtonGeom s_taskButtons[16];
static uint32_t s_taskButtonCount = 0;

static void BuildTaskbarButtons() {
    s_taskButtonCount = 0;
    if (!s_taskbar_enabled) return;
    const auto& fb = kos::gfx::GetInfo();
    // Leave space for a small "+" spawn button at the far left
    const uint32_t spawnW = 20;
    uint32_t x = kTaskbarPad + spawnW + 4;
    uint32_t y = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    for (uint32_t i=0;i<kos::ui::GetWindowCount() && s_taskButtonCount < 16; ++i) {
        uint32_t wid; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        // Skip non-closable utility windows like mouse diag (heuristic: title == "Mouse")
        if (d.title && (d.title[0]=='M' && d.title[1]=='o')) { continue; }
        TaskbarButtonGeom& b = s_taskButtons[s_taskButtonCount++];
        b.x = x; b.y = y; b.w = kTaskButtonW; b.h = kTaskButtonH; b.winId = wid; b.minimized = (st == kos::ui::WindowState::Minimized); b.focused = (wid == kos::ui::GetFocusedWindow());
        x += kTaskButtonW + 4;
        if (x + kTaskButtonW + kTaskbarPad > fb.width) break; // no more space
    }
}

static uint32_t TaskbarHitTest(int mx, int my) {
    if (!s_taskbar_enabled) return 0;
    const auto& fb = kos::gfx::GetInfo();
    if ((uint32_t)my < fb.height - kTaskbarHeight) return 0; // outside bar
    // Check spawn '+' button at far left
    const uint32_t spawnX = kTaskbarPad;
    const uint32_t spawnY = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    const uint32_t spawnW = 20;
    const uint32_t spawnH = kTaskButtonH;
    if ((uint32_t)mx >= spawnX && (uint32_t)mx < spawnX + spawnW && (uint32_t)my >= spawnY && (uint32_t)my < spawnY + spawnH) {
        return kTaskbarSpawnBtnId;
    }
    for (uint32_t i=0;i<s_taskButtonCount;++i) {
        const auto& b = s_taskButtons[i];
        if ((uint32_t)mx >= b.x && (uint32_t)mx < b.x + b.w && (uint32_t)my >= b.y && (uint32_t)my < b.y + b.h) {
            return b.winId;
        }
    }
    return 0;
}

uint32_t WindowManager::SpawnTerminal() {
    if (!kos::gfx::IsAvailable()) return 0;
    // Terminal geometry: 80x25 8x8 cells + chrome
    uint32_t termW = 80 * 8 + 16; // padding
    uint32_t termH = 25 * 8 + 18 + 8; // title + padding
    // Stagger new terminals slightly for visibility
    uint32_t x = 8 + (s_terminal_counter % 6) * 24;
    uint32_t y = 24 + (s_terminal_counter % 6) * 16;
    char title[32];
    // First terminal is just "Terminal", subsequent are "Terminal N"
    if (s_terminal_counter <= 1) {
        title[0]='T'; title[1]='e'; title[2]='r'; title[3]='m'; title[4]='i'; title[5]='n'; title[6]='a'; title[7]='l'; title[8]=0;
    } else {
        // Simple decimal append
        const char base[] = "Terminal ";
        int bi=0; while (base[bi]) { title[bi]=base[bi]; ++bi; }
        uint32_t n = s_terminal_counter;
        // convert n to decimal
        char rev[10]; int ri=0; if (n==0){ rev[ri++]='0'; }
        while(n && ri<10){ rev[ri++] = char('0' + (n%10)); n/=10; }
        while(ri){ title[bi++] = rev[--ri]; }
        title[bi]=0;
    }
    uint32_t newId = kos::ui::CreateWindow(x, y, termW, termH, 0xFF000000u, title);
    if (!newId) return 0;

    // Close previous terminal window if any, then rebind renderer to new window
    uint32_t oldId = kos::gfx::Terminal::GetWindowId();
    kos::gfx::Terminal::SetWindow(newId);
    kos::ui::BringToFront(newId);
    kos::ui::SetFocusedWindow(newId);
    // Ensure a clean buffer and fresh prompt (skip boot logs)
    kos::console::TTY::Clear();
    kos::console::TTY::DiscardPreinitBuffer();
    kos::gfx::Terminal::Clear();
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::Write((const int8_t*)"kos[00]:/");
    kos::console::TTY::SetColor(11, 0);
    kos::console::TTY::Write((const int8_t*)"");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::Write((const int8_t*)"$ ");

    // If there was a previous terminal window, close it to avoid an empty placeholder
    if (oldId && oldId != newId) {
        kos::ui::CloseWindow(oldId);
    }
    ++s_terminal_counter;
    return newId;
}

void WindowManager::SetMousePollMode(uint8_t mode) {
    if (mode > 2) mode = 2;
    s_mouse_poll_mode = mode;
}

bool WindowManager::Start() {
    if (!kos::gfx::IsAvailable()) {
        kos::console::Logger::Log("WindowManager: framebuffer not available; disabled");
        return false;
    }
    if (!kos::gfx::Compositor::Initialize()) {
        kos::console::Logger::Log("WindowManager: compositor init failed");
        return false;
    }
    kos::console::Logger::Log("WindowManager: compositor ready");
    // Suppress further log writes to TTY now; we'll unmute later in Tick() so
    // ServiceManager's status lines don't clobber the initial prompt.
    kos::console::Logger::MuteTTY(true);
    kos::ui::InitInput();
    // Center the cursor and set a comfortable sensitivity
    const auto& fb = kos::gfx::GetInfo();
    kos::ui::SetCursorPos((int)(fb.width/2), (int)(fb.height/2));
    kos::ui::SetMouseSensitivity(2, 1); // 2x default sensitivity
    kos::ui::Init();
    // Create a graphical terminal window sized for 80x25 8x8 cells + title bar
    {
        uint32_t termW = 80 * 8 + 16; // padding
        uint32_t termH = 25 * 8 + 18 + 8; // title + padding
        uint32_t termId = kos::ui::CreateWindow(8, 24, termW, termH, 0xFF000000u, "Terminal");
        if (termId) {
            kos::gfx::Terminal::Initialize(termId, 80, 25, false); // start with 8x8 for stability
            // Bring terminal to front and focus it so keyboard input works immediately
            kos::ui::BringToFront(termId);
            kos::ui::SetFocusedWindow(termId);
            // Discard any boot-time buffered output so GUI terminal starts clean
            kos::console::TTY::DiscardPreinitBuffer();
            kos::gfx::Terminal::Clear();
            // Do not unmute yet; allow ServiceManager to finish its post-start
            // logs offscreen. We'll unmute in Tick() after a short delay.
            // Emit a fresh shell prompt (mirrors basic Shell::PrintPrompt logic for root cwd)
            kos::console::TTY::SetAttr(0x07); // standard attribute
            kos::console::TTY::Write((const int8_t*)"kos[00]:/");
            kos::console::TTY::SetColor(11, 0); // light cyan for basename (root)
            kos::console::TTY::Write((const int8_t*)"");
            kos::console::TTY::SetAttr(0x07);
            kos::console::TTY::Write((const int8_t*)"$ ");
        }
    }
    // Create a small window to show live mouse coordinates
    {
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t w = 180, h = 44;
        uint32_t x = (fbinfo.width > w + 8 ? fbinfo.width - w - 8 : 8);
        uint32_t y = 24;
        s_mouse_win_id = kos::ui::CreateWindow(x, y, w, h, 0xFF0F0F10u, "Mouse");
        // Keep on top but do not steal focus from terminal
        if (s_mouse_win_id) {
            kos::ui::BringToFront(s_mouse_win_id);
            if (kos::gfx::Terminal::IsActive()) {
                kos::ui::SetFocusedWindow(kos::gfx::Terminal::GetWindowId());
            }
        }
    }
    // No demo window to avoid overlapping the terminal visually

    // Draw an initial frame immediately so the window is visible even before the service ticker runs
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(wallpaper);
    kos::ui::RenderAll();
    if (kos::gfx::Terminal::IsActive()) {
        kos::gfx::Terminal::Render();
    }
    // DEBUG OVERLAY (early): show fb + window stats if no windows visible
    {
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t winCount = kos::ui::GetWindowCount();
        bool term = kos::gfx::Terminal::IsActive();
        char buf[160];
        int bi=0; auto put=[&](char c){ if(bi<159) buf[bi++]=c; };
        auto writeStr=[&](const char* s){ while(*s) put(*s++); };
        auto writeDec=[&](uint32_t v){ char tmp[16]; int n=0; if(v==0){tmp[n++]='0';} else { char r[16]; int ri=0; while(v&&ri<16){ r[ri++]=char('0'+(v%10)); v/=10;} while(ri) tmp[n++]=r[--ri]; } for(int i=0;i<n;++i) put(tmp[i]); };
        writeStr("FB "); writeDec(fbinfo.width); put('x'); writeDec(fbinfo.height); put(' '); writeDec(fbinfo.bpp); writeStr("bpp  win:"); writeDec(winCount); writeStr(" term:"); writeStr(term?"Y":"N"); buf[bi]=0;
        // Draw line at top-left
        for (int i=0; buf[i]; ++i) {
            char ch = buf[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 4, glyph, 0xFFFFFFFFu, wallpaper);
        }
    }
    kos::gfx::Compositor::EndFrame();
    return true;
}

void WindowManager::Tick() {
    if (!kos::gfx::IsAvailable()) return;
    // One-time delayed logger unmute so boot logs and ServiceManager's status
    // don't clobber the initial prompt line in the GUI terminal.
    static bool s_unmuted = false;
    static uint32_t s_unmute_delay = 0;
    if (!s_unmuted) {
        if (++s_unmute_delay >= 2) { // wait ~2 ticks (~66ms) after Start
            kos::console::Logger::MuteTTY(false);
            s_unmuted = true;
        }
    }
    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    // Ensure terminal has focus at least once if no focus is set
    if (kos::ui::GetFocusedWindow() == 0 && kos::gfx::Terminal::IsActive()) {
        kos::ui::SetFocusedWindow(kos::gfx::Terminal::GetWindowId());
    }
    bool left = (mb & 1u) != 0;
    // Taskbar gets first dibs on clicks; otherwise defer to UI interactions
    if (left) {
        uint32_t tbWin = TaskbarHitTest(mx, my);
        if (tbWin == kTaskbarSpawnBtnId) {
            // Create a new terminal window on '+' button click
            WindowManager::SpawnTerminal();
        } else if (tbWin) {
            if (kos::ui::GetWindowState(tbWin) == kos::ui::WindowState::Minimized) {
                kos::ui::RestoreWindow(tbWin);
                kos::ui::BringToFront(tbWin);
                kos::ui::SetFocusedWindow(tbWin);
            } else {
                if (kos::ui::GetFocusedWindow() == tbWin) {
                    kos::ui::MinimizeWindow(tbWin);
                } else {
                    kos::ui::BringToFront(tbWin);
                    kos::ui::SetFocusedWindow(tbWin);
                }
            }
        } else {
            // Let UI process clicks/drags/resizes
            kos::ui::UpdateInteractions();
        }
    } else {
        // Still allow UI to finish interactions on mouse release
        kos::ui::UpdateInteractions();
    }
    // Consume a few UI events for diagnostics (kept lightweight to avoid spam)
    {
        kos::ui::UIEvent ev; int processed = 0; const int kMaxPerTick = 4;
        while (processed < kMaxPerTick && kos::ui::PollEvent(ev)) {
            ++processed;
            switch (ev.type) {
                case kos::ui::UIEventType::WindowClosed:
                    kos::console::Logger::Log("UI: window closed");
                    break;
                case kos::ui::UIEventType::WindowMinimized:
                    kos::console::Logger::Log("UI: window minimized");
                    break;
                case kos::ui::UIEventType::WindowRestored:
                    kos::console::Logger::Log("UI: window restored");
                    break;
                case kos::ui::UIEventType::WindowMaximized:
                    kos::console::Logger::Log("UI: window maximized");
                    break;
                case kos::ui::UIEventType::WindowFocused:
                case kos::ui::UIEventType::WindowMoved:
                case kos::ui::UIEventType::WindowResized:
                default:
                    // Skip verbose logs for frequent events
                    break;
            }
        }
    }
    // Poll mouse each tick (non-blocking). Keeps UI responsive on platforms where IRQ12 is unreliable.
    // Force polling until first packet is received, then respect configured mode.
    if (g_mouse_driver_ptr) {
        if (drivers::mouse::g_mouse_packets == 0) {
            g_mouse_driver_ptr->PollOnce();
        } else if (s_mouse_poll_mode == 2 || (s_mouse_poll_mode == 1 && drivers::mouse::g_mouse_packets == 0)) {
            g_mouse_driver_ptr->PollOnce();
        }
    }

    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(wallpaper);
    // Render all UI windows
    kos::ui::RenderAll();
    // Build taskbar model and render taskbar
    if (s_taskbar_enabled) {
        BuildTaskbarButtons();
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t barY = fbinfo.height - kTaskbarHeight;
        // Bar background
        kos::gfx::Compositor::FillRect(0, barY, fbinfo.width, kTaskbarHeight, 0xFF202022u);
        // Top separator line
        kos::gfx::Compositor::FillRect(0, barY, fbinfo.width, 2, 0xFF303033u);
        // Spawn '+' button at far left
        {
            uint32_t x = kTaskbarPad;
            uint32_t y = barY + (kTaskbarHeight - kTaskButtonH)/2;
            uint32_t w = 20, h = kTaskButtonH;
            uint32_t base = 0xFF2A2A2Du;
            kos::gfx::Compositor::FillRect(x, y, w, h, base);
            // Outline
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF000000u);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF000000u);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF000000u);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF000000u);
            // Draw '+' centered using 8x8 glyphs: '+' is ASCII 0x2B
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['+' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFFFFFu, base);
        }
        // Render buttons
        for (uint32_t i=0;i<s_taskButtonCount;++i) {
            const auto& b = s_taskButtons[i];
            uint32_t base = b.focused ? 0xFF3B82F6u : (b.minimized ? 0xFF2D2D30u : 0xFF2A2A2Du);
            kos::gfx::Compositor::FillRect(b.x, b.y, b.w, b.h, base);
            // Outline
            kos::gfx::Compositor::FillRect(b.x, b.y, b.w, 1, 0xFF000000u);
            kos::gfx::Compositor::FillRect(b.x, b.y + b.h - 1, b.w, 1, 0xFF000000u);
            kos::gfx::Compositor::FillRect(b.x, b.y, 1, b.h, 0xFF000000u);
            kos::gfx::Compositor::FillRect(b.x + b.w - 1, b.y, 1, b.h, 0xFF000000u);
            // Title glyphs truncated
            kos::gfx::WindowDesc wd; kos::ui::GetWindowDesc(b.winId, wd);
            const char* title = wd.title ? wd.title : "(untitled)";
            uint32_t maxChars = (b.w - 8) / 8; // padding left=4 right=4
            uint32_t tx = b.x + 4; uint32_t ty = b.y + (b.h/2) - 4;
            for (uint32_t ci=0; title[ci] && ci < maxChars; ++ci) {
                char ch = title[ci]; if (ch < 32 || ch > 127) ch='?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
                kos::gfx::Compositor::DrawGlyph8x8(tx + ci*8, ty, glyph, 0xFFFFFFFFu, base);
            }
        }
    }
    // Render mouse coordinate window contents (simple 8x8 text)
    if (s_mouse_win_id) {
        kos::gfx::WindowDesc d; 
        if (kos::ui::GetWindowDesc(s_mouse_win_id, d)) {
            int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
            // Build line 1: "x: ####  y: ####"
            char buf[64];
            int bi = 0; auto putc=[&](char c){ if (bi < (int)sizeof(buf)-1) buf[bi++]=c; };
            auto writeDec=[&](uint32_t v){ char tmp[16]; int n=0; if (v==0){ tmp[n++]='0'; } else { char r[16]; int ri=0; while(v && ri<16){ r[ri++]=char('0'+(v%10)); v/=10; } while(ri) tmp[n++]=r[--ri]; } for(int i=0;i<n;++i) putc(tmp[i]); };
            putc('x'); putc(':'); putc(' '); writeDec((uint32_t)mx); putc(' '); putc(' '); putc('y'); putc(':'); putc(' '); writeDec((uint32_t)my); buf[bi]=0;
            // Clear client area strip and draw text
            const uint32_t th = 18; const uint32_t padX = 6; const uint32_t padY = 6;
            uint32_t tx = d.x + padX; uint32_t ty = d.y + th + padY;
            // Draw background strip to avoid text ghosting
            uint32_t availW = (d.w > padX*2 ? d.w - padX*2 : d.w);
            uint32_t availH = (d.h > th + padY*2 ? d.h - th - padY*2 : 0);
            if (availH > 0) kos::gfx::Compositor::FillRect(tx, ty, availW, (availH < 24 ? availH : 24), d.bg);
            for (uint32_t i=0; buf[i]; ++i) {
                char ch = buf[i];
                if (ch < 32 || ch > 127) ch = '?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
                kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty, glyph, 0xFFFFFFFFu, d.bg);
            }

            // Build line 2: "btn: lmr  pk: #### src: IRQ|POLL" with uppercase when pressed
            bi = 0;
            putc('b'); putc('t'); putc('n'); putc(':'); putc(' ');
            bool left = (mb & 1u) != 0; bool right = (mb & 2u) != 0; bool middle = (mb & 4u) != 0;
            putc(left ? 'L' : 'l'); putc(middle ? 'M' : 'm'); putc(right ? 'R' : 'r');
            putc(' '); putc(' '); putc('p'); putc('k'); putc(':'); putc(' ');
            // Read packet counter
            uint32_t pk = drivers::mouse::g_mouse_packets;
            writeDec(pk);
            putc(' '); putc(' '); putc('s'); putc('r'); putc('c'); putc(':'); putc(' ');
            const char* src = (pk == 0 ? "POLL" : "IRQ");
            for (const char* s = src; *s; ++s) putc(*s);
            buf[bi]=0;
            uint32_t ty2 = ty + 10;
            for (uint32_t i=0; buf[i]; ++i) {
                char ch = buf[i]; if (ch < 32 || ch > 127) ch = '?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
                kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty2, glyph, 0xFFB0B0B0u, d.bg);
            }
        }
    }
    // Render terminal contents after windows so glyphs appear inside terminal client area
    if (kos::gfx::Terminal::IsActive()) {
        kos::gfx::Terminal::Render();
    }
    // DEBUG OVERLAY (live)
    {
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t winCount = kos::ui::GetWindowCount();
        bool term = kos::gfx::Terminal::IsActive();
        char buf[160];
        int bi=0; auto put=[&](char c){ if(bi<159) buf[bi++]=c; };
        auto writeStr=[&](const char* s){ while(*s) put(*s++); };
        auto writeDec=[&](uint32_t v){ char tmp[16]; int n=0; if(v==0){tmp[n++]='0';} else { char r[16]; int ri=0; while(v&&ri<16){ r[ri++]=char('0'+(v%10)); v/=10;} while(ri) tmp[n++]=r[--ri]; } for(int i=0;i<n;++i) put(tmp[i]); };
        writeStr("FB "); writeDec(fbinfo.width); put('x'); writeDec(fbinfo.height); put(' '); writeDec(fbinfo.bpp); writeStr(" win:"); writeDec(winCount); writeStr(" term:"); writeStr(term?"Y":"N"); writeStr(" foc:"); writeDec(kos::ui::GetFocusedWindow()); buf[bi]=0;
        for (int i=0; buf[i]; ++i) {
            char ch = buf[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 4, glyph, 0xFFFFFFFFu, wallpaper);
        }
    }
    // Optional animation: move the demo window horizontally (simple effect)
    // For now, leave static to keep it predictable.
    // Draw mouse cursor (simple crosshair) after all windows & terminal so it stays on top.
    {
        int cx, cy; uint8_t cb; kos::ui::GetMouseState(cx, cy, cb);
        // Clamp just in case (defensive; GetMouseState already clamps)
        const auto& fbinfo = kos::gfx::GetInfo();
        if (cx < 0) cx = 0; if (cy < 0) cy = 0;
        if (cx >= (int)fbinfo.width) cx = (int)fbinfo.width - 1;
        if (cy >= (int)fbinfo.height) cy = (int)fbinfo.height - 1;
        // Colors: white outline, inner depending on button state (left=red, middle=green, right=blue)
        uint32_t inner = 0xFFFFFFFFu;
        if (cb & 1u) inner = 0xFFFF4040u; // left
        else if (cb & 4u) inner = 0xFF40FF40u; // middle
        else if (cb & 2u) inner = 0xFF4040FFu; // right
        // Crosshair size
        // Horizontal line
        for (int dx=-3; dx<=3; ++dx) {
            int x = cx + dx; int y = cy; if (x>=0 && x<(int)fbinfo.width)
                kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dx==0? inner : 0xFFFFFFFFu));
        }
        // Vertical line
        for (int dy=-3; dy<=3; ++dy) {
            int x = cx; int y = cy + dy; if (y>=0 && y<(int)fbinfo.height)
                kos::gfx::Compositor::FillRect((uint32_t)x, (uint32_t)y, 1, 1, (dy==0? inner : 0xFFFFFFFFu));
        }
    }
    kos::gfx::Compositor::EndFrame();
}

}} // namespace
