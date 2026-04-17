#include <services/window_manager.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <ui/framework.hpp>
#include <ui/input.hpp>
#include <ui/login_screen.hpp>
#include <ui/process_viewer.hpp>
#include <graphics/terminal.hpp>
#include <graphics/font8x8_basic.hpp>
#include <services/service_manager.hpp>
#include <console/tty.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <kernel/globals.hpp>
#include <ui/cursor.hpp>
#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <drivers/ps2/ps2.hpp>
#include <lib/serial.hpp>
#include <lib/stdio.hpp>
#include <drivers/net/e1000/e1000_poll.h>
#include <process/thread_manager.hpp>
#include <process/scheduler.hpp>

// Use the canonical kernel global mouse driver pointer declared in `kernel/globals.hpp`.
// Access it as `kos::g_mouse_driver_ptr`.

using namespace kos::services;

namespace kos { namespace services {

static uint32_t t = 0;
static uint32_t s_mouse_win_id = 0; // small diagnostics window for mouse position
static uint32_t s_login_win_id = 0; // login window id during pre-login
static uint32_t s_process_monitor_win_id = 0; // process monitor window id
static bool s_login_active = false;
// 0=never, 1=until first packet (default), 2=always
static uint8_t s_mouse_poll_mode = 1;
static bool s_taskbar_enabled = true;
static uint32_t s_terminal_counter = 1; // for titling Shell N
static uint32_t s_prev_mouse_pk = 0;    // track mouse packet counter between frames
static int s_mouse_evt_flash = 0;       // frames to flash event indicator when new packets arrive
static uint32_t s_prev_kbd_ev = 0;      // track keyboard event counter
static int s_kbd_evt_flash = 0;         // frames to flash indicator when keys arrive
static bool s_shell_started = false;
static uint32_t s_clock_win_id = 0; // desktop clock/date widget
static uint32_t s_system_hud_win_id = 0; // desktop CPU/MEM/BAT widget
static uint32_t s_prev_total_runtime = 0;
static uint32_t s_prev_idle_runtime = 0;
static uint8_t  s_last_cpu_pct = 0;
static constexpr uint32_t kDesktopClockTopInset = 56u; // reserved top strip to avoid overlap with top widgets

static bool TryReadBatteryPercent(uint8_t& outPct) {
    int32_t pct = kos::sys::get_battery_percent();
    if (pct < 0 || pct > 100) return false;
    outPct = static_cast<uint8_t>(pct);
    return true;
}

static uint8_t blend8(uint8_t a, uint8_t b, uint32_t t, uint32_t max) {
    if (max == 0) return b;
    return static_cast<uint8_t>((static_cast<uint32_t>(a) * (max - t) + static_cast<uint32_t>(b) * t) / max);
}

static void FillCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell = 2) {
    if (cell == 0) cell = 1;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            uint32_t tx = (x + i) / cell;
            uint32_t ty = (y + j) / cell;
            uint32_t color = ((tx + ty) & 1u) ? cA : cB;
            kos::gfx::Compositor::FillRect(x + i, y + j, 1, 1, color);
        }
    }
}

static void RenderLoginBackdrop() {
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;

    constexpr uint32_t kTop = 0xFF140A3Eu;
    constexpr uint32_t kBottom = 0xFF2C1A68u;
    constexpr uint32_t kGlowA = 0xFF2E6BAAu;
    constexpr uint32_t kGlowB = 0xFF4A8DD2u;

    for (uint32_t y = 0; y < fb.height; ++y) {
        uint8_t r = blend8(static_cast<uint8_t>((kTop >> 16) & 0xFFu), static_cast<uint8_t>((kBottom >> 16) & 0xFFu), y, fb.height - 1);
        uint8_t g = blend8(static_cast<uint8_t>((kTop >> 8) & 0xFFu), static_cast<uint8_t>((kBottom >> 8) & 0xFFu), y, fb.height - 1);
        uint8_t b = blend8(static_cast<uint8_t>(kTop & 0xFFu), static_cast<uint8_t>(kBottom & 0xFFu), y, fb.height - 1);
        // Posterize channels to evoke a limited 8-bit palette.
        r = static_cast<uint8_t>(r & 0xE0u);
        g = static_cast<uint8_t>(g & 0xE0u);
        b = static_cast<uint8_t>(b & 0xC0u);
        if (y & 1u) {
            if (r > 8u) r = static_cast<uint8_t>(r - 8u);
            if (g > 8u) g = static_cast<uint8_t>(g - 8u);
            if (b > 8u) b = static_cast<uint8_t>(b - 8u);
        }
        kos::gfx::Compositor::FillRect(0, y, fb.width, 1, 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
                                                      (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b));
    }

    uint32_t glowY = (fb.height * 4u) / 5u;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t w = (fb.width / 6u) + i * (fb.width / 18u);
        uint32_t h = 10u + i * 6u;
        uint32_t x = (fb.width > w) ? ((fb.width - w) / 2u) : 0u;
        uint32_t y = (glowY > (i * 5u)) ? (glowY - i * 5u) : 0u;
        uint32_t color = (i < 2) ? kGlowB : kGlowA;
        kos::gfx::Compositor::FillRect(x, y, w, h, color);
    }

    for (uint32_t i = 0; i < 40; ++i) {
        uint32_t x = (i * 131u + 17u) % fb.width;
        uint32_t y = (i * 67u + 29u) % (fb.height > 120 ? fb.height - 120 : fb.height);
        kos::gfx::Compositor::FillRect(x, y, 1, 1, 0x66C7DDFFu);
    }
}

static void deferred_shell_thread_entry() {
    kos::console::ThreadedShellAPI::StartShell();
}
// File-scope keyboard handler that forwards keys to LoginScreen
class LoginKeyboardHandler : public kos::drivers::keyboard::KeyboardEventHandler {
public:
    virtual void OnKeyDown(int8_t c) override { 
        kos::ui::LoginScreen::OnKeyDown(c); 
    }
    virtual void OnKeyUp(int8_t) override {}
};
static LoginKeyboardHandler s_login_handler;

// Special taskbar hit ids
static constexpr uint32_t kTaskbarSpawnBtnId = 0xFFFFFFFFu; // sentinel for "+" button
static constexpr uint32_t kTaskbarRebootBtnId = 0xFFFFFFFEu; // sentinel for reboot button

extern "C" void app_reboot() __attribute__((weak));
extern "C" void app_shutdown() __attribute__((weak));

// Taskbar layout constants
static constexpr uint32_t kTaskbarHeight = 22; // bottom panel height
static constexpr uint32_t kTaskbarPad = 4;
static constexpr uint32_t kTaskButtonW = 120; // fixed width per window button
static constexpr uint32_t kTaskButtonH = 18;  // inside bar

static void TaskbarRebootSystem() {
    // Prefer the registered reboot app entrypoint used by the text shell.
    if (app_reboot) {
        app_reboot();
        return;
    }

    // Fallback: keyboard controller reset + triple-fault if needed.
    auto io_outb = [](uint16_t port, uint8_t val) {
        __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
    };
    auto io_inb = [](uint16_t port) -> uint8_t {
        uint8_t ret;
        __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    };

    while (io_inb(0x64) & 0x02u) { }
    io_outb(0x64, 0xFE);
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ __volatile__("lidt %0\n\tint $0x03" : : "m"(null_idt));
    for (;;) { __asm__ __volatile__("hlt"); }
}

static void TaskbarShutdownSystem() {
    if (app_shutdown) {
        app_shutdown();
        return;
    }
    for (;;) { __asm__ __volatile__("cli; hlt"); }
}

struct TaskbarButtonGeom { uint32_t x,y,w,h; uint32_t winId; bool minimized; bool focused; };
static TaskbarButtonGeom s_taskButtons[16];
static uint32_t s_taskButtonCount = 0;

static void BuildTaskbarButtons() {
    s_taskButtonCount = 0;
    if (!s_taskbar_enabled) return;
    const auto& fb = kos::gfx::GetInfo();
    // Leave space for a small "+" spawn button at the far left and reboot button at the far right.
    const uint32_t spawnW = 20;
    const uint32_t rebootW = 28;
    uint32_t x = kTaskbarPad + spawnW + 4;
    uint32_t y = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    for (uint32_t i=0;i<kos::ui::GetWindowCount() && s_taskButtonCount < 16; ++i) {
        uint32_t wid; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        // Skip non-closable utility windows (Mouse diag, Clock widget, System HUD)
        if (wid == s_mouse_win_id || wid == s_clock_win_id || wid == s_system_hud_win_id) { continue; }
        TaskbarButtonGeom& b = s_taskButtons[s_taskButtonCount++];
        b.x = x; b.y = y; b.w = kTaskButtonW; b.h = kTaskButtonH; b.winId = wid; b.minimized = (st == kos::ui::WindowState::Minimized); b.focused = (wid == kos::ui::GetFocusedWindow());
        x += kTaskButtonW + 4;
        if (x + kTaskButtonW + kTaskbarPad + rebootW + 4 > fb.width) break; // no more space
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
    // Reboot button at far right
    const uint32_t rebootW = 28;
    const uint32_t rebootX = fb.width - kTaskbarPad - rebootW;
    const uint32_t rebootY = spawnY;
    if ((uint32_t)mx >= rebootX && (uint32_t)mx < rebootX + rebootW && (uint32_t)my >= rebootY && (uint32_t)my < rebootY + spawnH) {
        return kTaskbarRebootBtnId;
    }
    for (uint32_t i=0;i<s_taskButtonCount;++i) {
        const auto& b = s_taskButtons[i];
        if ((uint32_t)mx >= b.x && (uint32_t)mx < b.x + b.w && (uint32_t)my >= b.y && (uint32_t)my < b.y + b.h) {
            return b.winId;
        }
    }
    return 0;
}

static void RenderMouseWindowContent() {
    if (!s_mouse_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_mouse_win_id, d)) return;

    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    // Update event flash based on packet counter
    uint32_t pk_now = drivers::mouse::g_mouse_packets;
    if (pk_now != s_prev_mouse_pk) { s_mouse_evt_flash = 15; s_prev_mouse_pk = pk_now; }
    else if (s_mouse_evt_flash > 0) { --s_mouse_evt_flash; }

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
    uint32_t maxChars = (availW / 8u);
    for (uint32_t i=0; buf[i] && i < maxChars; ++i) {
        char ch = buf[i];
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty, glyph, 0xFFFFFFFFu, d.bg);
    }

    // Build line 2: "btn: lmr  pk: #### src: IRQ|POLL cfg:0xHH [EVT]"
    bi = 0;
    putc('b'); putc('t'); putc('n'); putc(':'); putc(' ');
    bool left = (mb & 1u) != 0; bool right = (mb & 2u) != 0; bool middle = (mb & 4u) != 0;
    putc(left ? 'L' : 'l'); putc(middle ? 'M' : 'm'); putc(right ? 'R' : 'r');
    putc(' '); putc(' '); putc('p'); putc('k'); putc(':'); putc(' ');
    writeDec(pk_now);
    putc(' '); putc(' '); putc('s'); putc('r'); putc('c'); putc(':'); putc(' ');
    const char* src = (::kos::g_mouse_input_source == 2 ? "POLL" : (::kos::g_mouse_input_source == 1 ? "IRQ" : "-"));
    for (const char* s = src; *s; ++s) putc(*s);
    // Show PS/2 controller config byte
    putc(' '); putc(' '); putc('c'); putc('f'); putc('g'); putc(':');
    putc('0'); putc('x');
    auto& ps2 = ::kos::drivers::ps2::PS2Controller::Instance();
    uint8_t cfg = ps2.ReadConfig();
    const char* hex = "0123456789ABCDEF";
    putc(hex[(cfg>>4)&0xF]); putc(hex[cfg & 0xF]);
    if (s_mouse_evt_flash > 0) { putc(' '); putc(' '); putc('E'); putc('V'); putc('T'); }
    buf[bi]=0;
    uint32_t ty2 = ty + 10;
    for (uint32_t i=0; buf[i] && i < maxChars; ++i) {
        char ch = buf[i]; if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty2, glyph, 0xFFB0B0B0u, d.bg);
    }
}

// Draw a single 8x8 glyph at integer scale using FillRect per source pixel.
static void DrawGlyphScaled(uint32_t x, uint32_t y, const uint8_t* glyph,
                            uint32_t fg, uint32_t shadow, uint32_t scale) {
    if (scale == 0) scale = 1;
    for (uint32_t row = 0; row < 8u; ++row) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < 8u; ++col) {
            if (bits & (1u << col)) {
                uint32_t px = x + col * scale;
                uint32_t py = y + row * scale;
                if (shadow) kos::gfx::Compositor::FillRect(px + 1, py + 1, scale, scale, shadow);
                kos::gfx::Compositor::FillRect(px, py, scale, scale, fg);
            }
        }
    }
}

static void RenderClockWindowContent() {
    if (!s_clock_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_clock_win_id, d)) return;

    uint16_t year = 0; uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    kos::sys::get_datetime(&year, &month, &day, &hour, &minute, &second);

    auto w2 = [](char* buf, int& p, uint8_t v) {
        buf[p++] = char('0' + (v / 10u));
        buf[p++] = char('0' + (v % 10u));
    };
    auto w4 = [](char* buf, int& p, uint16_t v) {
        buf[p++] = char('0' + ((v / 1000u) % 10u));
        buf[p++] = char('0' + ((v / 100u)  % 10u));
        buf[p++] = char('0' + ((v / 10u)   % 10u));
        buf[p++] = char('0' + (v % 10u));
    };

    const char sep = ((second & 1u) == 0u) ? ':' : ' ';
    char timeBuf[12]; int tp = 0;
    w2(timeBuf, tp, hour); timeBuf[tp++] = sep;
    w2(timeBuf, tp, minute); timeBuf[tp++] = sep;
    w2(timeBuf, tp, second); timeBuf[tp] = 0;

    char dateBuf[16]; int dp = 0;
    w4(dateBuf, dp, year); dateBuf[dp++] = '-';
    w2(dateBuf, dp, month); dateBuf[dp++] = '-';
    w2(dateBuf, dp, day); dateBuf[dp] = 0;

    constexpr uint32_t kPhosphor       = 0xFF39FF14u;
    constexpr uint32_t kPhosphorDim    = 0xFF1A7A08u;
    constexpr uint32_t kPhosphorShadow = 0xFF041400u;
    constexpr uint32_t kBgA = 0xFF020A02u;
    constexpr uint32_t kBgB = 0xFF030C03u;
    constexpr uint32_t kBorderTop = 0xFF22AA22u;
    constexpr uint32_t kBorderBot = 0xFF001000u;
    constexpr uint32_t kScale = 2u;
    constexpr uint32_t kGlyphW2 = 8u * kScale;

    uint32_t cx = d.x + 1, cy = d.y + 1;
    uint32_t cw = d.w > 2 ? d.w - 2 : d.w;
    uint32_t ch = d.h > 2 ? d.h - 2 : d.h;

    for (uint32_t j = 0; j < ch; ++j) {
        for (uint32_t i = 0; i < cw; ++i) {
            uint32_t tx = (cx + i) / 2u;
            uint32_t ty2 = (cy + j) / 2u;
            kos::gfx::Compositor::FillRect(cx + i, cy + j, 1, 1, (((tx + ty2) & 1u) ? kBgA : kBgB));
        }
    }

    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderTop);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderBot);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderTop);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderBot);

    const uint32_t padY = 4u;
    uint32_t timeLen = static_cast<uint32_t>(tp);
    uint32_t timeRowW = timeLen * kGlyphW2;
    uint32_t timeX = (cw > timeRowW) ? cx + (cw - timeRowW) / 2u : cx + 4u;
    uint32_t timeY = cy + padY;
    for (uint32_t i = 0; i < timeLen; ++i) {
        char c = timeBuf[i];
        if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
        DrawGlyphScaled(timeX + i * kGlyphW2, timeY, glyph, kPhosphor, kPhosphorShadow, kScale);
    }

    uint32_t dateLen = static_cast<uint32_t>(dp);
    uint32_t dateRowW = dateLen * 8u;
    uint32_t dateX = (cw > dateRowW) ? cx + (cw - dateRowW) / 2u : cx + 4u;
    uint32_t dateY = timeY + kScale * 8u + 4u;
    for (uint32_t i = 0; i < dateLen; ++i) {
        char c = dateBuf[i];
        if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
        kos::gfx::Compositor::DrawGlyph8x8(dateX + i*8 + 1, dateY + 1, glyph, kPhosphorShadow, 0);
        kos::gfx::Compositor::DrawGlyph8x8(dateX + i*8, dateY, glyph, kPhosphorDim, 0);
    }
}

static void RenderSystemHudContent() {
    if (!s_system_hud_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_system_hud_win_id, d)) return;

    constexpr uint32_t kGreen    = 0xFF39FF14u;
    constexpr uint32_t kYellow   = 0xFFF2E85Cu;
    constexpr uint32_t kRed      = 0xFFFF5C5Cu;
    constexpr uint32_t kFgDim    = 0xFF1A7A08u;
    constexpr uint32_t kShadow   = 0xFF041400u;
    constexpr uint32_t kBgA      = 0xFF020A02u;
    constexpr uint32_t kBgB      = 0xFF030C03u;
    constexpr uint32_t kBorderHi = 0xFF22AA22u;
    constexpr uint32_t kBorderLo = 0xFF001000u;
    constexpr uint32_t kBarBg    = 0xFF0A170Au;

    uint32_t cx = d.x + 1, cy = d.y + 1;
    uint32_t cw = d.w > 2 ? d.w - 2 : d.w;
    uint32_t ch = d.h > 2 ? d.h - 2 : d.h;

    for (uint32_t j = 0; j < ch; ++j) {
        for (uint32_t i = 0; i < cw; ++i) {
            uint32_t tx = (cx + i) / 2u;
            uint32_t ty = (cy + j) / 2u;
            kos::gfx::Compositor::FillRect(cx + i, cy + j, 1, 1, (((tx + ty) & 1u) ? kBgA : kBgB));
        }
    }

    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderLo);

    uint32_t totalRuntime = 0;
    uint32_t idleRuntime = 0;
    if (kos::process::g_scheduler) {
        for (uint32_t tid = 1; tid <= 128; ++tid) {
            kos::process::Thread* task = kos::process::g_scheduler->FindTask(tid);
            if (!task || task->state == kos::process::TASK_TERMINATED) continue;
            totalRuntime += task->total_runtime;
            if (task->priority == kos::process::PRIORITY_IDLE) {
                idleRuntime += task->total_runtime;
            } else if (task->name && task->name[0] == 'i' && task->name[1] == 'd') {
                idleRuntime += task->total_runtime;
            }
        }
    }

    uint32_t dTotal = totalRuntime - s_prev_total_runtime;
    uint32_t dIdle = idleRuntime - s_prev_idle_runtime;
    if (dTotal > 0) {
        uint32_t busy = (dIdle <= dTotal) ? (dTotal - dIdle) : 0u;
        uint32_t pct = (busy * 100u) / dTotal;
        if (pct > 100u) pct = 100u;
        s_last_cpu_pct = static_cast<uint8_t>(pct);
    }
    s_prev_total_runtime = totalRuntime;
    s_prev_idle_runtime = idleRuntime;

    uint32_t totalFrames = 0;
    uint32_t freeFrames = 0;
    uint32_t heapSize = 0;
    uint32_t heapUsed = 0;
    if (kos::sys::table()) {
        if (kos::sys::table()->get_total_frames) totalFrames = kos::sys::table()->get_total_frames();
        if (kos::sys::table()->get_free_frames) freeFrames = kos::sys::table()->get_free_frames();
        if (kos::sys::table()->get_heap_size) heapSize = kos::sys::table()->get_heap_size();
        if (kos::sys::table()->get_heap_used) heapUsed = kos::sys::table()->get_heap_used();
    }
    uint32_t usedFrames = (totalFrames >= freeFrames) ? (totalFrames - freeFrames) : 0u;
    uint32_t memPct = (totalFrames > 0) ? ((usedFrames * 100u) / totalFrames) : 0u;

    auto colorByLoad = [&](uint32_t pct) -> uint32_t {
        if (pct >= 80u) return kRed;
        if (pct >= 50u) return kYellow;
        return kGreen;
    };

    uint8_t batPct = 0;
    bool hasBattery = TryReadBatteryPercent(batPct);
    if (batPct > 100u) batPct = 100u;

    auto drawText = [&](uint32_t x, uint32_t y, const char* text, uint32_t fg) {
        for (uint32_t i = 0; text[i]; ++i) {
            char c = text[i];
            if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(x + i*8 + 1, y + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(x + i*8, y, glyph, fg, 0);
        }
    };

    auto drawBar = [&](uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pct, uint32_t fg) {
        if (w < 2u || h < 2u) return;
        if (pct > 100u) pct = 100u;
        kos::gfx::Compositor::FillRect(x, y, w, h, kBarBg);
        kos::gfx::Compositor::FillRect(x, y, w, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, kBorderLo);
        kos::gfx::Compositor::FillRect(x, y, 1, h, kBorderHi);
        kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, kBorderLo);
        uint32_t fillW = (w > 2u) ? ((w - 2u) * pct) / 100u : 0u;
        if (fillW > 0u) {
            kos::gfx::Compositor::FillRect(x + 1u, y + 1u, fillW, h - 2u, fg);
        }
    };

    auto appendDec = [](char* out, int& p, uint32_t v) {
        char rev[12];
        int ri = 0;
        if (v == 0) rev[ri++] = '0';
        while (v && ri < 12) { rev[ri++] = char('0' + (v % 10u)); v /= 10u; }
        while (ri) out[p++] = rev[--ri];
    };

    char line1[40]; int p1 = 0;
    line1[p1++] = 'C'; line1[p1++] = 'P'; line1[p1++] = 'U'; line1[p1++] = ':'; line1[p1++] = ' ';
    appendDec(line1, p1, s_last_cpu_pct); line1[p1++] = '%'; line1[p1] = 0;

    char line2[64]; int p2 = 0;
    line2[p2++] = 'M'; line2[p2++] = 'E'; line2[p2++] = 'M'; line2[p2++] = ':'; line2[p2++] = ' ';
    appendDec(line2, p2, memPct); line2[p2++] = '%';
    line2[p2++] = ' '; line2[p2++] = 'H'; line2[p2++] = ':';
    appendDec(line2, p2, heapUsed / 1024u); line2[p2++] = '/'; appendDec(line2, p2, heapSize / 1024u); line2[p2++] = 'K';
    line2[p2] = 0;

    char line3[24]; int p3 = 0;
    line3[p3++] = 'B'; line3[p3++] = 'A'; line3[p3++] = 'T'; line3[p3++] = ':'; line3[p3++] = ' ';
    if (hasBattery) {
        appendDec(line3, p3, batPct); line3[p3++] = '%';
    } else {
        line3[p3++] = 'N'; line3[p3++] = '/'; line3[p3++] = 'A';
    }
    line3[p3] = 0;

    uint32_t tx = cx + 4u;
    uint32_t cpuColor = colorByLoad(s_last_cpu_pct);
    uint32_t memColor = colorByLoad(memPct);
    uint32_t batColor = hasBattery ? colorByLoad(batPct) : kFgDim;

    drawText(tx, cy + 2u, line1, cpuColor);
    drawBar(tx + 72u, cy + 3u, 96u, 6u, s_last_cpu_pct, cpuColor);

    drawText(tx, cy + 14u, line2, memColor);
    drawBar(tx + 72u, cy + 15u, 96u, 6u, memPct, memColor);

    drawText(tx, cy + 26u, line3, batColor);
    drawBar(tx + 72u, cy + 27u, 96u, 6u, hasBattery ? static_cast<uint32_t>(batPct) : 0u, batColor);
}

static void RenderWindowContentsByZOrder() {
    uint32_t count = kos::ui::GetWindowCount();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t wid = 0; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl = 0;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        if (st == kos::ui::WindowState::Minimized) continue;

        // Global content validator: clip all window content to its client area.
        uint32_t cx = d.x + 1;
        uint32_t cy = d.y + ((fl & kos::ui::WF_Frameless) ? 1u : (kos::ui::TitleBarHeight() + 1u));
        uint32_t cw = (d.w > 2 ? d.w - 2 : 0);
        uint32_t ch = 0;
        if (fl & kos::ui::WF_Frameless) {
            ch = (d.h > 2 ? d.h - 2 : 0);
        } else {
            uint32_t chrome = kos::ui::TitleBarHeight() + 2;
            ch = (d.h > chrome ? d.h - chrome : 0);
        }
        kos::gfx::Compositor::SetClipRect(cx, cy, cw, ch);

        if (wid == s_process_monitor_win_id) {
            kos::ui::ProcessViewer::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (s_login_active && wid == s_login_win_id) {
            kos::ui::LoginScreen::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_mouse_win_id) {
            RenderMouseWindowContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_clock_win_id) {
            RenderClockWindowContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_system_hud_win_id) {
            RenderSystemHudContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (!s_login_active && kos::gfx::Terminal::IsActive() && wid == kos::gfx::Terminal::GetWindowId()) {
            kos::gfx::Terminal::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        kos::gfx::Compositor::ClearClipRect();
    }
}

static void CreatePostLoginWindows() {
    if (s_clock_win_id == 0) {
        const auto& fb = kos::gfx::GetInfo();
        // "HH:MM:SS" = 8 chars * 16px (2x) = 128px + 14px padding
        // height: 4 + 16 + 4 + 8 + 4 = 36px
        uint32_t cw = 142, ch = 36;
        uint32_t cx = (fb.width > cw + 8u) ? fb.width - cw - 8u : 0u;
        uint32_t cy = 8u;
        s_clock_win_id = kos::ui::CreateWindowEx(cx, cy, cw, ch, 0xFF020A02u, "Clock",
                             kos::ui::WindowRole::Utility,
                             kos::ui::WF_Frameless);
    }

    if (s_system_hud_win_id == 0) {
        const auto& fb = kos::gfx::GetInfo();
        const uint32_t hudW = 176u, hudH = 44u;
        const uint32_t clockW = 142u;
        uint32_t hx = 8u;
        if (fb.width > (clockW + hudW + 24u)) {
            hx = fb.width - clockW - hudW - 16u;
        }
        s_system_hud_win_id = kos::ui::CreateWindowEx(hx, 8u, hudW, hudH, 0xFF020A02u, "SystemHUD",
                                  kos::ui::WindowRole::Utility,
                                  kos::ui::WF_Frameless);
    }

    // Reserve a top strip so auto-positioned windows won't overlap the clock widget.
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }

    if (s_mouse_win_id == 0) {
        uint32_t w = 280, h = 56;
        s_mouse_win_id = kos::ui::CreateWindowEx(kos::ui::kAutoCoord, kos::ui::kAutoCoord, w, h, 0xFF0F0F10u, "Mouse",
                             kos::ui::WindowRole::Utility,
                             kos::ui::WF_Minimizable | kos::ui::WF_Closable);
        if (s_mouse_win_id) {
            kos::ui::BringToFront(s_mouse_win_id);
        }
    }

    if (s_process_monitor_win_id == 0) {
        kos::gfx::Rect area;
        kos::ui::GetWorkArea(area);
        uint32_t w = (area.w > 900 ? (area.w * 7) / 10 : (area.w > 220 ? area.w - 40 : area.w));
        uint32_t h = (area.h > 560 ? (area.h * 3) / 4 : (area.h > 120 ? area.h - 32 : area.h));
        s_process_monitor_win_id = kos::ui::CreateWindowEx(kos::ui::kAutoCoord, kos::ui::kAutoCoord, w, h, 0xFF1F1F2E, "System Process Monitor",
                                 kos::ui::WindowRole::Normal,
                                 kos::ui::WF_Resizable | kos::ui::WF_Minimizable |
                                 kos::ui::WF_Maximizable | kos::ui::WF_Closable);
        if (s_process_monitor_win_id) {
            kos::ui::ProcessViewer::Initialize(s_process_monitor_win_id);
            kos::ui::ProcessViewer::RefreshProcessList();
        }
    }
}

uint32_t WindowManager::SpawnTerminal() {
    if (!kos::gfx::IsAvailable()) return 0;
    // Terminal geometry: 80x25 8x8 cells + chrome
    uint32_t termW = 80 * 8 + 16; // padding
    uint32_t termH = 25 * 8 + 18 + 8; // title + padding
    char title[32];
    // First window is "Shell", subsequent are "Shell N"
    if (s_terminal_counter <= 1) {
        title[0]='S'; title[1]='h'; title[2]='e'; title[3]='l'; title[4]='l'; title[5]=0;
    } else {
        // Simple decimal append
        const char base[] = "Shell ";
        int bi=0; while (base[bi]) { title[bi]=base[bi]; ++bi; }
        uint32_t n = s_terminal_counter;
        // convert n to decimal
        char rev[10]; int ri=0; if (n==0){ rev[ri++]='0'; }
        while(n && ri<10){ rev[ri++] = char('0' + (n%10)); n/=10; }
        while(ri){ title[bi++] = rev[--ri]; }
        title[bi]=0;
    }
    uint32_t newId = kos::ui::CreateWindow(kos::ui::kAutoCoord, kos::ui::kAutoCoord, termW, termH, 0xFF000000u, title);
    if (!newId) return 0;

    // Close previous terminal window if any, then rebind renderer to new window.
    // Ensure the terminal backend is initialized before rendering/writing to it.
    uint32_t oldId = kos::gfx::Terminal::GetWindowId();
    if (!kos::gfx::Terminal::IsActive()) {
        if (!kos::gfx::Terminal::Initialize(newId, 80, 25, true)) {
            kos::ui::CloseWindow(newId);
            return 0;
        }
    } else {
        kos::gfx::Terminal::SetWindow(newId);
    }
    kos::ui::BringToFront(newId);
    kos::ui::SetFocusedWindow(newId);
    // Ensure a clean buffer and fresh prompt (skip boot logs)
    kos::console::TTY::Clear();
    kos::console::TTY::DiscardPreinitBuffer();
    kos::gfx::Terminal::Clear();
    kos::console::TTY::SetColor(10, 0);
    kos::console::TTY::Write((const int8_t*)"[GUI SHELL] ");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::Write((const int8_t*)"Mismo prompt y comandos que el entorno texto\n");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::SetColor(14, 0);
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
    // Input subsystem was initialized earlier at BootStage::InputInit
    kos::ui::Init();
    // Define desktop work area (reserve bottom strip for taskbar/panel).
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = s_login_active ? 0u : kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }
    // Create a login window first, before any terminal. This is blocking.
    {
        uint32_t w = 320, h = 380;
        s_login_win_id = kos::ui::CreateWindowEx(kos::ui::kAutoCoord, kos::ui::kAutoCoord, w, h, 0xFF101012u, "Login",
                     kos::ui::WindowRole::Dialog, kos::ui::WF_Frameless, 0);
        if (s_login_win_id) {
            s_login_active = true;
            kos::ui::BringToFront(s_login_win_id);
            kos::ui::SetFocusedWindow(s_login_win_id);
            kos::ui::LoginScreen::Initialize(s_login_win_id);
            // Swap keyboard handler to capture login input - use global override for reliability
            ::kos::g_keyboard_handler_override = &s_login_handler;
            
            // Also try to set via driver if available
            if (::kos::g_keyboard_driver_ptr) {
                ::kos::g_keyboard_driver_ptr->SetHandler(&s_login_handler);
            }
        }
    }
    // Desktop windows are created only after successful login.
    s_mouse_win_id = 0;
    s_process_monitor_win_id = 0;
    s_clock_win_id = 0;
    s_system_hud_win_id = 0;

    // Draw an initial frame immediately so the window is visible even before the service ticker runs
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(s_login_active ? 0xFF16061Eu : wallpaper);
    if (s_login_active) RenderLoginBackdrop();
    kos::ui::RenderAll();
    // Render each window client strictly by z-order to keep stacking coherent.
    RenderWindowContentsByZOrder();
    
    // *** STARTUP TEST: Write to serial to confirm it's working ***
    static bool startup_logged = false;
    if (!startup_logged) {
        kos::lib::serial_init();
        kos::lib::serial_write("\n[WM] Window manager initialized\n");
        startup_logged = true;
    }
    
    // DEBUG OVERLAY (early): show fb + window stats if no windows visible
    if (!s_login_active) {
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
        // Second line: input sources (kbd/mouse) and brief key indicator
        char buf2[160]; bi=0;
        auto writeStr2 = [&](const char* s){ while(*s) buf2[bi++]=*s++; };
        auto writeSrc=[&](const char* label, uint8_t src){ writeStr2(label); writeStr2(":"); if(src==1) writeStr2("IRQ"); else if(src==2) writeStr2("POLL"); else writeStr2("-"); writeStr2("  "); };
        writeSrc("kbd", ::kos::g_kbd_input_source);
        writeSrc("mouse", ::kos::g_mouse_input_source);
        // Update keyboard event flash
        if (::kos::g_kbd_events != s_prev_kbd_ev) { s_kbd_evt_flash = 15; s_prev_kbd_ev = ::kos::g_kbd_events; }
        else if (s_kbd_evt_flash > 0) { --s_kbd_evt_flash; }
        if (s_kbd_evt_flash > 0) { writeStr2("[KEY]"); }
        buf2[bi]=0;
        for (int i=0; buf2[i]; ++i) {
            char ch = buf2[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 14, glyph, 0xFFFFFFFFu, wallpaper);
        }
    }
    kos::gfx::Compositor::EndFrame();
    return true;
}

void WindowManager::Tick() {
    // Poll E1000 for received packets
    e1000_rx_poll();
    
    if (!kos::gfx::IsAvailable()) return;
    // Keep work area in sync with current resolution/panel reservation.
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = s_login_active ? 0u : kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }
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
    // Ensure focus exists: prefer login when active, else terminal
    if (kos::ui::GetFocusedWindow() == 0) {
        if (s_login_active && s_login_win_id) kos::ui::SetFocusedWindow(s_login_win_id);
        else if (kos::gfx::Terminal::IsActive()) kos::ui::SetFocusedWindow(kos::gfx::Terminal::GetWindowId());
    }
    bool left = (mb & 1u) != 0;
    static bool s_prev_left = false;
    // Blocking login mode: no desktop interaction until auth succeeds.
    if (s_login_active) {
        if (s_login_win_id) {
            kos::ui::BringToFront(s_login_win_id);
            kos::ui::SetFocusedWindow(s_login_win_id);
        }
        if (left && !s_prev_left) {
            kos::ui::LoginScreen::OnPointerDown(mx, my);
        }
        // Let login issue power actions
        kos::ui::LoginAction la = kos::ui::LoginScreen::ConsumePendingAction();
        if (la == kos::ui::LoginAction::Reboot) {
            TaskbarRebootSystem();
        } else if (la == kos::ui::LoginAction::Shutdown) {
            TaskbarShutdownSystem();
        }
    } else {
        // Taskbar gets first dibs on clicks; otherwise defer to UI interactions
        if (left) {
            uint32_t tbWin = TaskbarHitTest(mx, my);
            if (tbWin == kTaskbarSpawnBtnId) {
                WindowManager::SpawnTerminal();
            } else if (tbWin == kTaskbarRebootBtnId) {
                TaskbarRebootSystem();
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
                kos::ui::UpdateInteractions();
            }
        } else {
            kos::ui::UpdateInteractions();
        }
    }
    s_prev_left = left;
    // Ensure login keyboard handler stays active while login is shown
    // Set handler EVERY tick to ensure it stays correct even if something else changes it
    if (s_login_active) {
        // Always set the global override - this is the most reliable method
        ::kos::g_keyboard_handler_override = &s_login_handler;
        if (::kos::g_keyboard_driver_ptr) {
            ::kos::g_keyboard_driver_ptr->SetHandler(&s_login_handler);
        }
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
    // Poll mouse as a fallback only. Avoid mixing IRQ + poll streams because that can desync pointer motion.
    static uint32_t s_no_pkt_ticks = 0;
    static bool s_notified_no_irq = false;
    static bool s_seen_irq_mouse = false;
    static uint32_t s_ticks_since_irq = 0;
    if (::kos::g_mouse_driver_ptr) {
        if (::kos::g_mouse_input_source == 1) {
            s_seen_irq_mouse = true;
            s_ticks_since_irq = 0;
        } else if (s_ticks_since_irq < 0xFFFFFFFFu) {
            ++s_ticks_since_irq;
        }

        bool needPoll = false;
        if (s_mouse_poll_mode == 2) {
            // Always mode: use poll until IRQ is seen, then only if IRQ has been silent for a while.
            needPoll = (!s_seen_irq_mouse) || (s_ticks_since_irq > 8);
        } else if (s_mouse_poll_mode == 1) {
            // Compatibility mode: poll only until IRQ is seen.
            needPoll = !s_seen_irq_mouse;
        } else {
            // Never mode
            needPoll = false;
        }

        if (needPoll) {
            for (int i = 0; i < 16; ++i) {
                uint32_t before = drivers::mouse::g_mouse_packets;
                ::kos::g_mouse_driver_ptr->PollOnce();
                auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
                uint8_t status = ps2.ReadStatus();
                if ((status & 0x01) == 0 || (status & 0x20) == 0) {
                    break;
                }
                if (drivers::mouse::g_mouse_packets != before) {
                    break;
                }
            }
        }
        if (drivers::mouse::g_mouse_packets == 0) {
            if (!s_notified_no_irq) {
                if (++s_no_pkt_ticks >= 60) { // ~2 seconds at 33ms/tick
                    kos::console::Logger::Log("WINMAN: no mouse IRQ packets yet; polling fallback active");
                    s_notified_no_irq = true;
                }
            }
        } else {
            s_no_pkt_ticks = 0; s_notified_no_irq = true; // we have packets; suppress notice
        }
    }
    // Keyboard fallback polling in graphics mode: avoid mixing IRQ + poll paths.
    // Use polling only until IRQ input is seen, or if IRQ goes silent for a while.
    static bool s_seen_irq_kbd = false;
    static uint32_t s_ticks_since_kbd_irq = 0;
    if (::kos::g_kbd_input_source == 1) {
        s_seen_irq_kbd = true;
        s_ticks_since_kbd_irq = 0;
    } else if (s_ticks_since_kbd_irq < 0xFFFFFFFFu) {
        ++s_ticks_since_kbd_irq;
    }

    bool needKbdPoll = (!s_seen_irq_kbd) || (s_ticks_since_kbd_irq > 8);
    if (::kos::g_keyboard_driver_ptr && needKbdPoll) {
        for (int i = 0; i < 16 && ::kos::g_keyboard_driver_ptr->PollOnce(); ++i) {
            // Drain buffered keyboard bytes while in fallback mode.
        }
    }

    // Input watchdog: if both keyboard and mouse remain idle for several seconds,
    // re-assert PS/2 enable/reporting commands to recover from lost controller state.
    {
        static uint32_t s_prev_kbd_ev_wd = 0;
        static uint32_t s_prev_mouse_pk_wd = 0;
        static uint32_t s_idle_ticks = 0;

        const uint32_t curK = ::kos::g_kbd_events;
        const uint32_t curM = ::kos::drivers::mouse::g_mouse_packets;
        if (curK == s_prev_kbd_ev_wd && curM == s_prev_mouse_pk_wd) {
            ++s_idle_ticks;
        } else {
            s_idle_ticks = 0;
            s_prev_kbd_ev_wd = curK;
            s_prev_mouse_pk_wd = curM;
        }

        if (s_idle_ticks >= 150) { // ~5 seconds at 30Hz
            s_idle_ticks = 0;
            auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();

            // Ensure both PS/2 ports are enabled
            ps2.WaitWrite(); ps2.WriteCommand(0xAE); // enable keyboard port
            ps2.WaitWrite(); ps2.WriteCommand(0xA8); // enable mouse port

            // Ensure controller command byte keeps IRQ1/IRQ12 and both clocks enabled
            ps2.WaitWrite(); ps2.WriteCommand(0x20);
            ps2.WaitRead();
            uint8_t cfg = ps2.ReadData();
            cfg |= 0x03;    // IRQ1 + IRQ12
            cfg &= ~0x30u;  // enable clocks for ports 1 and 2
            ps2.WaitWrite(); ps2.WriteCommand(0x60);
            ps2.WaitWrite(); ps2.WriteData(cfg);

            // Re-enable keyboard scanning
            ps2.WaitWrite(); ps2.WriteData(0xF4);
            for (int i = 0; i < 50000; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    (void)ps2.ReadData();
                    break;
                }
            }

            // Re-enable mouse reporting
            ps2.WaitWrite(); ps2.WriteCommand(0xD4);
            ps2.WaitWrite(); ps2.WriteData(0xF4);
            for (int i = 0; i < 50000; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    (void)ps2.ReadData();
                    break;
                }
            }
        }
    }

    // Refresh process list every ~30 ticks (for smooth updates)
    if (s_process_monitor_win_id) {
        static uint32_t refresh_counter = 0;
        if (++refresh_counter >= 30) {
            kos::ui::ProcessViewer::RefreshProcessList();
            refresh_counter = 0;
        }
    }

    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(s_login_active ? 0xFF16061Eu : wallpaper);
    if (s_login_active) RenderLoginBackdrop();
    // Render frame and contents in z-order
    kos::ui::RenderAll();
    RenderWindowContentsByZOrder();

    // Login transition (state logic kept after rendering)
    if (s_login_active) {
        // If authenticated, transition to normal terminal UI
        if (kos::ui::LoginScreen::Authenticated()) {
            // Close login window
            if (s_login_win_id) { kos::ui::CloseWindow(s_login_win_id); s_login_win_id = 0; }
            s_login_active = false;
            // Restore keyboard handler to shell - clear global override first
            ::kos::g_keyboard_handler_override = nullptr;
            if (!s_shell_started) {
                if (kos::console::ThreadedShellAPI::InitializeShell()) {
                    uint32_t shellTid = kos::process::ThreadManagerAPI::CreateSystemThread(
                        (void*)deferred_shell_thread_entry,
                        kos::process::THREAD_SYSTEM_SERVICE, 8192, kos::process::PRIORITY_NORMAL, "shell");
                    if (shellTid) {
                        s_shell_started = true;
                        kos::console::ThreadedShellAPI::ProcessKeyInput('\n');
                    }
                }
            }
            static kos::console::ShellKeyboardHandler s_shell_handler;
            if (::kos::g_keyboard_driver_ptr) {
                ::kos::g_keyboard_driver_ptr->SetHandler(&s_shell_handler);
            }
            // Create desktop windows only after successful authentication.
            CreatePostLoginWindows();
            // Spawn terminal now
            WindowManager::SpawnTerminal();
        }
    }
    // Build taskbar model and render taskbar only after login.
    if (s_taskbar_enabled && !s_login_active) {
        BuildTaskbarButtons();
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t barY = fbinfo.height - kTaskbarHeight;
        // Retro 8-bit taskbar palette.
        FillCheckerRect(0, barY, fbinfo.width, kTaskbarHeight, 0xFF122A52u, 0xFF153160u, 2);
        kos::gfx::Compositor::FillRect(0, barY, fbinfo.width, 1, 0xFF8FC2FFu);
        kos::gfx::Compositor::FillRect(0, barY + kTaskbarHeight - 1, fbinfo.width, 1, 0xFF00183Cu);
        // Spawn '+' button at far left
        {
            uint32_t x = kTaskbarPad;
            uint32_t y = barY + (kTaskbarHeight - kTaskButtonH)/2;
            uint32_t w = 20, h = kTaskButtonH;
            uint32_t base = 0xFF1E3E78u;
            FillCheckerRect(x, y, w, h, base, 0xFF22488Au, 2);
            // Outline
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF00183Cu);
            // Draw '+' centered using 8x8 glyphs: '+' is ASCII 0x2B
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['+' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx + 1, gy + 1, glyph, 0xFF0A1020u, base);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFE26Fu, base);
        }
        // Reboot button at far right
        {
            uint32_t w = 28, h = kTaskButtonH;
            uint32_t x = fbinfo.width - kTaskbarPad - w;
            uint32_t y = barY + (kTaskbarHeight - h)/2;
            uint32_t base = 0xFF1E3E78u;
            FillCheckerRect(x, y, w, h, base, 0xFF22488Au, 2);
            // Outline
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF00183Cu);
            // Draw "R" centered.
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['R' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx + 1, gy + 1, glyph, 0xFF0A1020u, base);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFE26Fu, base);
        }
        // Render buttons
        for (uint32_t i=0;i<s_taskButtonCount;++i) {
            const auto& b = s_taskButtons[i];
            uint32_t base = b.focused ? 0xFF2E5AA8u : (b.minimized ? 0xFF1A2D4Eu : 0xFF1E3E78u);
            uint32_t alt = b.focused ? 0xFF3364B8u : (b.minimized ? 0xFF1D345Au : 0xFF22488Au);
            FillCheckerRect(b.x, b.y, b.w, b.h, base, alt, 2);
            // Outline
            kos::gfx::Compositor::FillRect(b.x, b.y, b.w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(b.x, b.y + b.h - 1, b.w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(b.x, b.y, 1, b.h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(b.x + b.w - 1, b.y, 1, b.h, 0xFF00183Cu);
            // Title glyphs truncated
            kos::gfx::WindowDesc wd; kos::ui::GetWindowDesc(b.winId, wd);
            const char* title = wd.title ? wd.title : "(untitled)";
            uint32_t maxChars = (b.w - 8) / 8; // padding left=4 right=4
            uint32_t tx = b.x + 4; uint32_t ty = b.y + (b.h/2) - 4;
            for (uint32_t ci=0; title[ci] && ci < maxChars; ++ci) {
                char ch = title[ci]; if (ch < 32 || ch > 127) ch='?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
                kos::gfx::Compositor::DrawGlyph8x8(tx + ci*8 + 1, ty + 1, glyph, 0xFF0A1020u, base);
                kos::gfx::Compositor::DrawGlyph8x8(tx + ci*8, ty, glyph, 0xFFFFE26Fu, base);
            }
        }
    }
    // DEBUG OVERLAY (live)
    if (!s_login_active) {
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
    // Render cursor (style selectable) after all content
    kos::ui::RenderCursor();
    kos::gfx::Compositor::EndFrame();
}

}} // namespace
