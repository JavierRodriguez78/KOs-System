#include <ui/system_components.hpp>
#include <input/event_queue.hpp>
#include <ui/framework.hpp>
#include <graphics/compositor.hpp>
#include <graphics/font8x8_basic.hpp>
#include <lib/stdio.hpp>
#include <process/scheduler.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/ps2/ps2.hpp>
#include <ui/input.hpp>
#include <kernel/globals.hpp>

namespace kos { namespace ui {

static void FillCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell) {
    if (cell == 0) cell = 1;
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t i = 0;
        while (i < w) {
            uint32_t tx = (x + i) / cell;
            uint32_t ty = (y + j) / cell;
            uint32_t color = ((tx + ty) & 1u) ? cA : cB;
            uint32_t segLen = 1;
            while (i + segLen < w) {
                uint32_t nextTx = (x + i + segLen) / cell;
                uint32_t nextColor = ((nextTx + ty) & 1u) ? cA : cB;
                if (nextColor != color) break;
                ++segLen;
            }
            kos::gfx::Compositor::FillRect(x + i, y + j, segLen, 1, color);
            i += segLen;
        }
    }
}

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

// ============================================================================
// Clock Component
// ============================================================================

void ClockComponent::Render() {
    const uint32_t wid = GetWindowId();
    if (!wid) return;

    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(wid, d)) return;

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

    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

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

bool ClockComponent::OnInputEvent(const input::InputEvent& event) {
    // Clock is read-only, doesn't handle input
    return false;
}

// ============================================================================
// SystemHUD Component
// ============================================================================

void SystemHudComponent::Render() {
    const uint32_t wid = GetWindowId();
    if (!wid) return;

    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(wid, d)) return;

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

    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

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

    uint32_t dTotal = totalRuntime - prev_total_runtime_;
    uint32_t dIdle = idleRuntime - prev_idle_runtime_;
    if (dTotal > 0) {
        uint32_t busy = (dIdle <= dTotal) ? (dTotal - dIdle) : 0u;
        uint32_t pct = (busy * 100u) / dTotal;
        if (pct > 100u) pct = 100u;
        last_cpu_pct_ = static_cast<uint8_t>(pct);
    }
    prev_total_runtime_ = totalRuntime;
    prev_idle_runtime_ = idleRuntime;

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
    int32_t batRaw = kos::sys::get_battery_percent();
    bool hasBattery = (batRaw >= 0 && batRaw <= 100);
    if (hasBattery) batPct = static_cast<uint8_t>(batRaw);

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
    appendDec(line1, p1, last_cpu_pct_); line1[p1++] = '%'; line1[p1] = 0;

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
    uint32_t cpuColor = colorByLoad(last_cpu_pct_);
    uint32_t memColor = colorByLoad(memPct);
    uint32_t batColor = hasBattery ? colorByLoad(batPct) : kFgDim;

    drawText(tx, cy + 2u, line1, cpuColor);
    drawBar(tx + 72u, cy + 3u, 96u, 6u, last_cpu_pct_, cpuColor);

    drawText(tx, cy + 14u, line2, memColor);
    drawBar(tx + 72u, cy + 15u, 96u, 6u, memPct, memColor);

    drawText(tx, cy + 26u, line3, batColor);
    drawBar(tx + 72u, cy + 27u, 96u, 6u, hasBattery ? static_cast<uint32_t>(batPct) : 0u, batColor);
}

bool SystemHudComponent::OnInputEvent(const input::InputEvent& event) {
    // SystemHUD is read-only, doesn't handle input
    return false;
}

// ============================================================================
// HardwareInfo Component
// ============================================================================

void HardwareInfoComponent::Render() {
    // TODO: Move RenderHardwareInfoWindowContent() rendering logic here
}

bool HardwareInfoComponent::OnInputEvent(const input::InputEvent& event) {
    // TODO: Handle tab clicks, pagination, etc.
    return false;
}

void HardwareInfoComponent::OnWindowResized(uint32_t width, uint32_t height) {
    // Mark for redraw on resize
    InvalidateContent();
}

// ============================================================================
// FileBrowser Component
// ============================================================================

void FileBrowserComponent::Render() {
    // TODO: Move RenderFileBrowserContent() rendering logic here
}

bool FileBrowserComponent::OnInputEvent(const input::InputEvent& event) {
    // TODO: Handle file selection, navigation, etc.
    return false;
}

void FileBrowserComponent::OnWindowResized(uint32_t width, uint32_t height) {
    // Mark for redraw on resize
    InvalidateContent();
}

// ============================================================================
// Process Monitor Component
// ============================================================================

void ProcessMonitorComponent::Render() {
    // TODO: Delegate to kos::ui::ProcessViewer::Render()
}

bool ProcessMonitorComponent::OnInputEvent(const input::InputEvent& event) {
    // TODO: Handle process selection, sorting, etc.
    return false;
}

void ProcessMonitorComponent::OnWindowResized(uint32_t width, uint32_t height) {
    // Mark for redraw on resize
    InvalidateContent();
}

// ============================================================================
// Mouse Diagnostic Component
// ============================================================================

void MouseDiagnosticComponent::Render() {
    const uint32_t wid = GetWindowId();
    if (!wid) return;

    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(wid, d)) return;

    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);

    uint32_t pk_now = kos::drivers::mouse::g_mouse_packets;
    if (pk_now != last_seen_packets_) {
        last_seen_packets_ = pk_now;
        event_flash_frames_ = 15;
    } else if (event_flash_frames_ > 0) {
        --event_flash_frames_;
    }

    char buf[64];
    int bi = 0;
    auto putc = [&](char c){ if (bi < (int)sizeof(buf)-1) buf[bi++] = c; };
    auto writeDec = [&](uint32_t v){
        char tmp[16]; int n = 0;
        if (v == 0) { tmp[n++] = '0'; }
        else {
            char r[16]; int ri = 0;
            while (v && ri < 16) { r[ri++] = char('0' + (v % 10)); v /= 10; }
            while (ri) tmp[n++] = r[--ri];
        }
        for (int i = 0; i < n; ++i) putc(tmp[i]);
    };

    putc('x'); putc(':'); putc(' '); writeDec((uint32_t)mx);
    putc(' '); putc(' '); putc('y'); putc(':'); putc(' '); writeDec((uint32_t)my);
    buf[bi] = 0;

    const uint32_t th = kos::ui::TitleBarHeight();
    const uint32_t padX = 6u;
    const uint32_t padY = 6u;
    uint32_t tx = d.x + padX;
    uint32_t ty = d.y + th + padY;
    uint32_t availW = (d.w > padX*2 ? d.w - padX*2 : d.w);
    uint32_t availH = (d.h > th + padY*2 ? d.h - th - padY*2 : 0);
    if (availH > 0) kos::gfx::Compositor::FillRect(tx, ty, availW, (availH < 24 ? availH : 24), d.bg);
    uint32_t maxChars = (availW / 8u);
    for (uint32_t i = 0; buf[i] && i < maxChars; ++i) {
        char ch = buf[i]; if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty, glyph, 0xFFFFFFFFu, d.bg);
    }

    bi = 0;
    putc('b'); putc('t'); putc('n'); putc(':'); putc(' ');
    bool left = (mb & 1u) != 0; bool right = (mb & 2u) != 0; bool middle = (mb & 4u) != 0;
    putc(left ? 'L' : 'l'); putc(middle ? 'M' : 'm'); putc(right ? 'R' : 'r');
    putc(' '); putc(' '); putc('p'); putc('k'); putc(':'); putc(' ');
    writeDec(pk_now);
    putc(' '); putc(' '); putc('s'); putc('r'); putc('c'); putc(':'); putc(' ');
    const char* src = (::kos::g_mouse_input_source == 2 ? "POLL" : (::kos::g_mouse_input_source == 1 ? "IRQ" : "-"));
    for (const char* s = src; *s; ++s) putc(*s);
    putc(' '); putc(' '); putc('c'); putc('f'); putc('g'); putc(':');
    putc('0'); putc('x');
    auto& ps2 = ::kos::drivers::ps2::PS2Controller::Instance();
    uint8_t cfg = ps2.ReadConfig();
    const char* hex = "0123456789ABCDEF";
    putc(hex[(cfg >> 4) & 0xF]); putc(hex[cfg & 0xF]);
    if (event_flash_frames_ > 0) { putc(' '); putc(' '); putc('E'); putc('V'); putc('T'); }
    buf[bi] = 0;

    uint32_t ty2 = ty + 10;
    for (uint32_t i = 0; buf[i] && i < maxChars; ++i) {
        char ch = buf[i]; if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        kos::gfx::Compositor::DrawGlyph8x8(tx + i*8, ty2, glyph, 0xFFB0B0B0u, d.bg);
    }
}

bool MouseDiagnosticComponent::OnInputEvent(const input::InputEvent& event) {
    if (event.type == input::EventType::MouseMove ||
        event.type == input::EventType::MousePress ||
        event.type == input::EventType::MouseRelease) {
        event_flash_frames_ = 15;
        return true;
    }
    return false;
}

}}  // namespace kos::ui
