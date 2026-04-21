#include <ui/system_components.hpp>
#include <input/event_queue.hpp>
#include <ui/framework.hpp>
#include <ui/process_viewer.hpp>
#include <fs/filesystem.hpp>
#include <graphics/compositor.hpp>
#include <graphics/font8x8_basic.hpp>
#include <lib/stdio.hpp>
#include <process/scheduler.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/ps2/ps2.hpp>
#include <drivers/gpu/vmsvga.hpp>
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

static void BufPutChar(char* out, uint32_t outSize, uint32_t& p, char c) {
    if (p + 1u >= outSize) return;
    out[p++] = c;
}

static void BufPutStr(char* out, uint32_t outSize, uint32_t& p, const char* s) {
    if (!s) return;
    while (*s) {
        if (p + 1u >= outSize) break;
        out[p++] = *s++;
    }
}

static void BufPutDec(char* out, uint32_t outSize, uint32_t& p, uint32_t v) {
    char rev[12];
    uint32_t ri = 0;
    if (v == 0u) rev[ri++] = '0';
    while (v && ri < sizeof(rev)) {
        rev[ri++] = static_cast<char>('0' + (v % 10u));
        v /= 10u;
    }
    while (ri) BufPutChar(out, outSize, p, rev[--ri]);
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

const char* HardwareInfoComponent::kTabTitles[kHwTabs] = {
    "Overview", "CPU", "Graphics", "Memory", "Storage", "Network", "PCI", "Controllers"
};

static void HwBufPutHexFixed(char* out, uint32_t outSize, uint32_t& p, uint32_t v, uint32_t digits) {
    static const char* kHex = "0123456789ABCDEF";
    if (digits == 0u) return;
    for (int32_t i = static_cast<int32_t>(digits) - 1; i >= 0; --i) {
        uint32_t shift = static_cast<uint32_t>(i) * 4u;
        BufPutChar(out, outSize, p, kHex[(v >> shift) & 0xFu]);
    }
}

static void HwBufPadTo(char* out, uint32_t outSize, uint32_t& p, uint32_t col) {
    while (p < col) BufPutChar(out, outSize, p, ' ');
}

uint32_t HardwareInfoComponent::PciCfgRead(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) const {
    return kos::sys::pci_cfg_read(bus, device, function, offset);
}

void HardwareInfoComponent::AppendLine(const char* line) {
    if (line_count_ >= 24u || line == nullptr) return;
    char* dst = lines_[line_count_++];
    uint32_t i = 0;
    while (line[i] && i < 111u) { dst[i] = line[i]; ++i; }
    dst[i] = 0;
}

void HardwareInfoComponent::AppendKV(const char* key, const char* value) {
    if (!key) key = "";
    if (!value) value = "-";
    char line[112];
    kos::sys::snprintf(line, sizeof(line), "%s\t%s", key, value);
    AppendLine(line);
}

void HardwareInfoComponent::FormatPciLocation(char* out, uint32_t outSize, uint8_t bus, uint8_t dev, uint8_t fn) const {
    uint32_t p = 0;
    HwBufPutHexFixed(out, outSize, p, bus, 2);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, fn);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

void HardwareInfoComponent::FormatPciId(char* out, uint32_t outSize, uint16_t vendor, uint16_t device) const {
    uint32_t p = 0;
    HwBufPutHexFixed(out, outSize, p, vendor, 4);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, device, 4);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

void HardwareInfoComponent::FormatPciSummary(char* out, uint32_t outSize, const char* vendorName,
                                             uint8_t bus, uint8_t dev, uint8_t fn,
                                             uint16_t vendor, uint16_t device) const {
    uint32_t p = 0;
    BufPutStr(out, outSize, p, vendorName);
    BufPutStr(out, outSize, p, " | pci ");
    HwBufPutHexFixed(out, outSize, p, bus, 2);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, fn);
    BufPutStr(out, outSize, p, " | ");
    HwBufPutHexFixed(out, outSize, p, vendor, 4);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, device, 4);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

void HardwareInfoComponent::FormatPciTableRow(char* out, uint32_t outSize, const PciListEntry& e) const {
    uint32_t p = 0;
    HwBufPutHexFixed(out, outSize, p, e.bus, 2);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, e.dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, e.fn);
    HwBufPadTo(out, outSize, p, 9u);
    HwBufPutHexFixed(out, outSize, p, e.vendor, 4);
    BufPutChar(out, outSize, p, ':');
    HwBufPutHexFixed(out, outSize, p, e.device, 4);
    HwBufPadTo(out, outSize, p, 19u);
    HwBufPutHexFixed(out, outSize, p, e.cls, 2);
    BufPutChar(out, outSize, p, '/');
    HwBufPutHexFixed(out, outSize, p, e.subclass, 2);
    HwBufPadTo(out, outSize, p, 25u);
    HwBufPutHexFixed(out, outSize, p, e.progIf, 2);
    HwBufPadTo(out, outSize, p, 30u);
    BufPutStr(out, outSize, p, PciVendorName(e.vendor, e.device));
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

const char* HardwareInfoComponent::PciClassName(uint8_t cls, uint8_t subclass) const {
    if (cls == 0x01u) {
        if (subclass == 0x01u) return "IDE";
        if (subclass == 0x06u) return "SATA";
        if (subclass == 0x08u) return "NVMe";
        return "Storage";
    }
    if (cls == 0x02u) return "Network";
    if (cls == 0x03u) return "Display";
    if (cls == 0x0Cu) return "SerialBus";
    return "Other";
}

const char* HardwareInfoComponent::PciVendorName(uint16_t vendor, uint16_t device) const {
    if (vendor == 0x8086u) {
        if (device == 0x100Eu) return "Intel e1000";
        return "Intel";
    }
    if (vendor == 0x10ECu) {
        if (device == 0x8169u || device == 0x8168u) return "Realtek RTL816x";
        if (device == 0x8139u) return "Realtek RTL8139";
        if (device == 0xB822u || device == 0xB828u) return "Realtek RTL8822BE";
        return "Realtek";
    }
    if (vendor == 0x15ADu) return "VMware";
    if (vendor == 0x1234u) return "QEMU";
    if (vendor == 0x80EEu) return "VirtualBox";
    return "Unknown";
}

uint32_t HardwareInfoComponent::PciClassRowColor(uint8_t cls) const {
    switch (cls) {
        case 0x01: return 0xFFFFD27Au;
        case 0x02: return 0xFF8BF7B2u;
        case 0x03: return 0xFF8EC8FFu;
        case 0x0C: return 0xFF9EE5E7u;
        default:   return 0xFFEAF4FFu;
    }
}

void HardwareInfoComponent::ScanPciClass(uint8_t wantedClass, PciDeviceSummary& first, uint32_t& count) {
    first = {false, 0, 0, 0, 0, 0, 0};
    count = 0;
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            uint8_t headerType = static_cast<uint8_t>(PciCfgRead(bus, dev, 0, 0x0E));
            uint8_t functions = (headerType & 0x80u) ? 8u : 1u;
            for (uint8_t fn = 0; fn < functions; ++fn) {
                uint16_t vendor = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x00));
                if (vendor == 0xFFFFu) { if (fn == 0u) break; continue; }
                uint8_t cls = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0B));
                if (cls != wantedClass) continue;
                ++count;
                if (!first.valid) {
                    first.valid = true;
                    first.bus = bus;
                    first.dev = dev;
                    first.fn = fn;
                    first.vendor = vendor;
                    first.device = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x02));
                    first.subclass = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0A));
                }
            }
        }
    }
}

void HardwareInfoComponent::ScanPciAllDevices() {
    pci_entry_count_ = 0;
    for (uint8_t bus = 0; bus < 8 && pci_entry_count_ < 96u; ++bus) {
        for (uint8_t dev = 0; dev < 32 && pci_entry_count_ < 96u; ++dev) {
            uint8_t headerType = static_cast<uint8_t>(PciCfgRead(bus, dev, 0, 0x0E));
            uint8_t functions = (headerType & 0x80u) ? 8u : 1u;
            for (uint8_t fn = 0; fn < functions && pci_entry_count_ < 96u; ++fn) {
                uint16_t vendor = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x00));
                if (vendor == 0xFFFFu) { if (fn == 0u) break; continue; }
                PciListEntry& e = pci_entries_[pci_entry_count_++];
                e.bus = bus; e.dev = dev; e.fn = fn;
                e.vendor = vendor;
                e.device = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x02));
                e.cls = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0B));
                e.subclass = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0A));
                e.progIf = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x09));
            }
        }
    }
}

void HardwareInfoComponent::CpuidRegs(uint32_t eaxIn, uint32_t ecxIn, uint32_t regs[4]) const {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(eaxIn), "c"(ecxIn));
}

const char* HardwareInfoComponent::InputSourceName(uint8_t src) const {
    if (src == 1u) return "IRQ";
    if (src == 2u) return "POLL";
    return "none";
}

void HardwareInfoComponent::RefreshSnapshot() {
    uint32_t regs[4] = {0, 0, 0, 0};
    for (int i = 0; i < 13; ++i) snapshot_.cpuVendor[i] = 0;
    for (int i = 0; i < 49; ++i) snapshot_.cpuBrand[i] = 0;

    CpuidRegs(0, 0, regs);
    uint32_t maxBasic = regs[0];
    reinterpret_cast<uint32_t*>(snapshot_.cpuVendor)[0] = regs[1];
    reinterpret_cast<uint32_t*>(snapshot_.cpuVendor)[1] = regs[3];
    reinterpret_cast<uint32_t*>(snapshot_.cpuVendor)[2] = regs[2];
    snapshot_.cpuVendor[12] = 0;

    CpuidRegs(0x80000000u, 0, regs);
    uint32_t maxExt = regs[0];
    if (maxExt >= 0x80000004u) {
        uint32_t* p = reinterpret_cast<uint32_t*>(snapshot_.cpuBrand);
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
            CpuidRegs(leaf, 0, regs);
            *p++ = regs[0]; *p++ = regs[1]; *p++ = regs[2]; *p++ = regs[3];
        }
    }

    snapshot_.cpuFamily = 0;
    snapshot_.cpuModel = 0;
    snapshot_.cpuStepping = 0;
    snapshot_.cpuLogical = 1;
    if (maxBasic >= 1u) {
        CpuidRegs(1, 0, regs);
        uint32_t eax = regs[0], ebx = regs[1];
        snapshot_.cpuStepping = eax & 0xFu;
        snapshot_.cpuModel = (eax >> 4) & 0xFu;
        snapshot_.cpuFamily = (eax >> 8) & 0xFu;
        uint32_t extModel = (eax >> 16) & 0xFu;
        uint32_t extFamily = (eax >> 20) & 0xFFu;
        if (snapshot_.cpuFamily == 0xFu) snapshot_.cpuFamily += extFamily;
        if (snapshot_.cpuFamily == 0x6u || snapshot_.cpuFamily == 0xFu) snapshot_.cpuModel += (extModel << 4);
        snapshot_.cpuLogical = (ebx >> 16) & 0xFFu;
        if (snapshot_.cpuLogical == 0u) snapshot_.cpuLogical = 1u;
    }

    snapshot_.totalFrames = 0;
    snapshot_.freeFrames = 0;
    snapshot_.heapSize = 0;
    snapshot_.heapUsed = 0;
    if (kos::sys::table()) {
        if (kos::sys::table()->get_total_frames) snapshot_.totalFrames = kos::sys::table()->get_total_frames();
        if (kos::sys::table()->get_free_frames) snapshot_.freeFrames = kos::sys::table()->get_free_frames();
        if (kos::sys::table()->get_heap_size) snapshot_.heapSize = kos::sys::table()->get_heap_size();
        if (kos::sys::table()->get_heap_used) snapshot_.heapUsed = kos::sys::table()->get_heap_used();
    }

    ScanPciClass(0x03u, snapshot_.display, snapshot_.displayCount);
    ScanPciClass(0x01u, snapshot_.storage, snapshot_.storageCount);
    ScanPciClass(0x02u, snapshot_.network, snapshot_.networkCount);
    ScanPciAllDevices();

    snapshot_.vmsvgaReady = kos::drivers::gpu::vmsvga::IsReady();
    snapshot_.renderBackend = kos::gfx::Compositor::BackendName();
    snapshot_.keyboardDriverReady = (::kos::g_keyboard_driver_ptr != nullptr);
    snapshot_.mouseDriverReady = (::kos::g_mouse_driver_ptr != nullptr);
    snapshot_.keyboardInput = InputSourceName(::kos::g_kbd_input_source);
    snapshot_.mouseInput = InputSourceName(::kos::g_mouse_input_source);
    snapshot_.mousePackets = ::kos::drivers::mouse::g_mouse_packets;
    snapshot_.filesystemMounted = (::kos::fs::g_fs_ptr != nullptr);
}

void HardwareInfoComponent::BuildLinesForTab(HardwareTab tab) {
    line_count_ = 0;
    char buf[112];

    uint32_t usedFrames = (snapshot_.totalFrames >= snapshot_.freeFrames) ? (snapshot_.totalFrames - snapshot_.freeFrames) : 0u;
    uint32_t totalMiB = snapshot_.totalFrames / 256u;
    uint32_t freeMiB = snapshot_.freeFrames / 256u;
    uint32_t usedMiB = usedFrames / 256u;

    switch (tab) {
        case HardwareTab::Overview:
            AppendLine("System Hardware Dashboard");
            AppendKV("CPU", snapshot_.cpuBrand);
            kos::sys::snprintf(buf, sizeof(buf), "%s | %u logical cores", snapshot_.cpuVendor, snapshot_.cpuLogical);
            AppendKV("CPU Detail", buf);
            if (snapshot_.display.valid) {
                FormatPciSummary(buf, sizeof(buf), PciVendorName(snapshot_.display.vendor, snapshot_.display.device),
                                 snapshot_.display.bus, snapshot_.display.dev, snapshot_.display.fn,
                                 snapshot_.display.vendor, snapshot_.display.device);
                AppendKV("Graphics", buf);
            } else {
                AppendKV("Graphics", "No PCI display adapter");
            }
            kos::sys::snprintf(buf, sizeof(buf), "%u/%u MiB used  (%u MiB free)", usedMiB, totalMiB, freeMiB);
            AppendKV("Memory", buf);
            break;
        case HardwareTab::CPU:
            AppendLine("CPU Information");
            AppendKV("Model", snapshot_.cpuBrand);
            AppendKV("Vendor", snapshot_.cpuVendor);
            kos::sys::snprintf(buf, sizeof(buf), "%u", snapshot_.cpuFamily); AppendKV("Family", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u", snapshot_.cpuModel); AppendKV("Model ID", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u", snapshot_.cpuStepping); AppendKV("Stepping", buf);
            break;
        case HardwareTab::Graphics:
            AppendLine("Graphics Adapter");
            AppendKV("Render Backend", snapshot_.renderBackend);
            AppendKV("VMSVGA", snapshot_.vmsvgaReady ? "Ready" : "Disabled");
            break;
        case HardwareTab::Memory:
            AppendLine("Memory State");
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", totalMiB); AppendKV("Physical Total", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", usedMiB); AppendKV("Physical Used", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", freeMiB); AppendKV("Physical Free", buf);
            break;
        case HardwareTab::Storage:
            AppendLine("Storage Controllers");
            if (snapshot_.storage.valid) AppendKV("Controller", PciVendorName(snapshot_.storage.vendor, snapshot_.storage.device));
            else AppendKV("Controller", "No PCI storage controller");
            break;
        case HardwareTab::Network:
            AppendLine("Network Adapters");
            if (snapshot_.network.valid) AppendKV("Primary Adapter", PciVendorName(snapshot_.network.vendor, snapshot_.network.device));
            else AppendKV("Primary Adapter", "No PCI network adapter");
            break;
        case HardwareTab::PCI: {
            AppendLine("PCI Device Inventory");
            uint32_t pageCount = (pci_entry_count_ + kPciPageSize - 1u) / kPciPageSize;
            if (pageCount == 0u) pageCount = 1u;
            if (pci_page_ >= pageCount) pci_page_ = pageCount - 1u;
            kos::sys::snprintf(buf, sizeof(buf), "%u device(s)  page %u/%u", pci_entry_count_, pci_page_ + 1u, pageCount);
            AppendKV("Summary", buf);
            AppendLine("BDF      VID:DID   CLS   IF   VENDOR");
            AppendLine("--------------------------------------------");
            uint32_t start = pci_page_ * kPciPageSize;
            uint32_t end = start + kPciPageSize;
            if (end > pci_entry_count_) end = pci_entry_count_;
            for (uint32_t i = start; i < end; ++i) {
                FormatPciTableRow(buf, sizeof(buf), pci_entries_[i]);
                AppendLine(buf);
            }
            break;
        }
        case HardwareTab::Controllers:
            AppendLine("Controllers and Runtime Drivers");
            AppendKV("Keyboard Driver", snapshot_.keyboardDriverReady ? "Loaded" : "Not loaded");
            AppendKV("Mouse Driver", snapshot_.mouseDriverReady ? "Loaded" : "Not loaded");
            AppendKV("Keyboard Input", snapshot_.keyboardInput);
            AppendKV("Mouse Input", snapshot_.mouseInput);
            break;
        default:
            break;
    }
}

bool HardwareInfoComponent::GetTabLayout(const kos::gfx::WindowDesc& d,
                                         uint32_t& cx, uint32_t& cy, uint32_t& cw, uint32_t& ch,
                                         uint32_t& tabX, uint32_t& tabY, uint32_t& tabW) const {
    const uint32_t flags = kos::ui::GetWindowFlags(GetWindowId());
    const bool frameless = (flags & kos::ui::WF_Frameless) != 0u;
    cx = d.x + 1u;
    cy = d.y + (frameless ? 1u : (kos::ui::TitleBarHeight() + 1u));
    cw = (d.w > 2u) ? d.w - 2u : d.w;
    if (frameless) ch = (d.h > 2u) ? d.h - 2u : d.h;
    else {
        const uint32_t chrome = kos::ui::TitleBarHeight() + 2u;
        ch = (d.h > chrome) ? (d.h - chrome) : 0u;
    }
    if (cw < 120u || ch < 80u) return false;
    tabX = cx + 6u;
    tabY = cy + 4u;
    uint32_t availW = (cw > 12u) ? (cw - 12u) : 0u;
    if (availW < (kHwTabs * 40u)) return false;
    tabW = availW / kHwTabs;
    if (tabW < 44u) tabW = 44u;
    return true;
}

bool HardwareInfoComponent::HandleTabClick(int mx, int my) {
    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != GetWindowId()) return false;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(GetWindowId(), d)) return false;
    uint32_t cx, cy, cw, ch, tabX, tabY, tabW;
    if (!GetTabLayout(d, cx, cy, cw, ch, tabX, tabY, tabW)) return false;
    uint32_t ux = static_cast<uint32_t>(mx), uy = static_cast<uint32_t>(my);
    if (ux < tabX || ux >= tabX + tabW * kHwTabs || uy < tabY || uy >= tabY + kHwTabStripH) return false;
    uint32_t idx = (ux - tabX) / tabW;
    if (idx >= kHwTabs) return false;
    if (static_cast<HardwareTab>(idx) != active_tab_) pci_page_ = 0;
    active_tab_ = static_cast<HardwareTab>(idx);
    BuildLinesForTab(active_tab_);
    return true;
}

bool HardwareInfoComponent::HandlePciPagerClick(int mx, int my) {
    if (active_tab_ != HardwareTab::PCI || !pci_btns_visible_) return false;
    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != GetWindowId()) return false;
    auto inRect = [&](const kos::gfx::Rect& r) -> bool {
        uint32_t ux = static_cast<uint32_t>(mx), uy = static_cast<uint32_t>(my);
        return (ux >= r.x && ux < r.x + r.w && uy >= r.y && uy < r.y + r.h);
    };
    uint32_t pageCount = (pci_entry_count_ + kPciPageSize - 1u) / kPciPageSize;
    if (pageCount == 0u) pageCount = 1u;
    if (inRect(pci_prev_btn_) && pci_page_ > 0u) { --pci_page_; BuildLinesForTab(HardwareTab::PCI); return true; }
    if (inRect(pci_next_btn_) && (pci_page_ + 1u) < pageCount) { ++pci_page_; BuildLinesForTab(HardwareTab::PCI); return true; }
    return false;
}

void HardwareInfoComponent::Render() {
    const uint32_t wid = GetWindowId();
    if (!wid) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(wid, d)) return;

    if (refresh_ticks_ == 0u || ++refresh_ticks_ >= 45u) {
        RefreshSnapshot();
        BuildLinesForTab(active_tab_);
        refresh_ticks_ = 1u;
    }

    constexpr uint32_t kBgA = 0xFF0B1220u;
    constexpr uint32_t kBgB = 0xFF0E1628u;
    constexpr uint32_t kPanel = 0xFF101D35u;
    constexpr uint32_t kBorderHi = 0xFF6EA1EAu;
    constexpr uint32_t kBorderLo = 0xFF163863u;
    constexpr uint32_t kFg = 0xFFEAF4FFu;
    constexpr uint32_t kLabel = 0xFF9FC0E8u;
    constexpr uint32_t kShadow = 0xFF04070Fu;
    constexpr uint32_t kBtn = 0xFF1B3258u;

    uint32_t cx, cy, cw, ch, tabX, tabY, tabW;
    if (!GetTabLayout(d, cx, cy, cw, ch, tabX, tabY, tabW)) return;

    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);
    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderLo);

    for (uint32_t ti = 0; ti < kHwTabs; ++ti) {
        uint32_t tx = tabX + ti * tabW;
        bool active = (ti == static_cast<uint32_t>(active_tab_));
        uint32_t a = active ? 0xFF2F5CA0u : 0xFF1A2A49u;
        uint32_t b = active ? 0xFF3A6DB8u : 0xFF1D3258u;
        FillCheckerRect(tx, tabY, tabW - 1u, kHwTabStripH, a, b, 2);
        kos::gfx::Compositor::FillRect(tx, tabY, tabW - 1u, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(tx, tabY + kHwTabStripH - 1u, tabW - 1u, 1, kBorderLo);
        const char* name = kTabTitles[ti];
        uint32_t len = 0; while (name[len]) ++len;
        uint32_t nameX = tx + ((tabW > len * 8u) ? ((tabW - len * 8u) / 2u) : 2u);
        for (uint32_t c = 0; name[c]; ++c) {
            char chv = name[c]; if (chv < 32 || chv > 127) chv = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[chv - 32];
            kos::gfx::Compositor::DrawGlyph8x8(nameX + c * 8u + 1u, tabY + 6u, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(nameX + c * 8u, tabY + 5u, glyph, active ? 0xFFFFFFFFu : 0xFFC7DCF7u, 0);
        }
    }

    uint32_t panelX = cx + 6u;
    uint32_t panelY = tabY + kHwTabStripH + 6u;
    uint32_t panelW = (cw > 12u) ? (cw - 12u) : 0u;
    uint32_t panelH = (cy + ch > panelY + 6u) ? ((cy + ch) - panelY - 6u) : 0u;
    if (panelW < 10u || panelH < 10u) return;
    FillCheckerRect(panelX, panelY, panelW, panelH, kPanel, 0xFF13233Fu, 2);
    kos::gfx::Compositor::FillRect(panelX, panelY, panelW, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(panelX, panelY + panelH - 1u, panelW, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(panelX, panelY, 1, panelH, kBorderHi);
    kos::gfx::Compositor::FillRect(panelX + panelW - 1u, panelY, 1, panelH, kBorderLo);

    pci_btns_visible_ = false;
    pci_prev_btn_ = {0, 0, 0, 0};
    pci_next_btn_ = {0, 0, 0, 0};
    if (active_tab_ == HardwareTab::PCI) {
        uint32_t btnW = 54u, btnH = 12u;
        uint32_t by = (panelY + panelH > btnH + 3u) ? (panelY + panelH - btnH - 3u) : panelY;
        uint32_t nextX = (panelX + panelW > btnW + 6u) ? (panelX + panelW - btnW - 6u) : panelX;
        uint32_t prevX = (nextX > btnW + 6u) ? (nextX - btnW - 6u) : panelX;
        pci_prev_btn_ = {prevX, by, btnW, btnH};
        pci_next_btn_ = {nextX, by, btnW, btnH};
        pci_btns_visible_ = true;
        FillCheckerRect(prevX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        FillCheckerRect(nextX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
    }

    uint32_t textX = panelX + 6u;
    uint32_t textY = panelY + 6u;
    uint32_t valueX = panelX + (panelW > 280u ? panelW / 3u : 98u);
    uint32_t rowH = 10u;
    uint32_t footerReserve = (active_tab_ == HardwareTab::PCI) ? 16u : 0u;
    uint32_t maxRows = (panelH > (12u + footerReserve)) ? ((panelH - 12u - footerReserve) / rowH) : 0u;
    uint32_t maxLabelChars = (valueX > textX + 8u) ? ((valueX - textX - 8u) / 8u) : 0u;
    uint32_t maxValueChars = (panelX + panelW > valueX + 8u) ? ((panelX + panelW - valueX - 8u) / 8u) : 0u;

    for (uint32_t row = 0; row < line_count_ && row < maxRows; ++row) {
        const char* text = lines_[row];
        const char* split = nullptr;
        for (const char* p = text; *p; ++p) { if (*p == '\t') { split = p; break; } }
        uint32_t y = textY + row * rowH;
        if (!split) {
            uint32_t rowColor = (active_tab_ == HardwareTab::PCI && row >= 4u)
                ? PciClassRowColor(pci_entries_[pci_page_ * kPciPageSize + (row - 4u)].cls)
                : 0xFFFFFFFFu;
            for (uint32_t i = 0; text[i] && i < maxValueChars; ++i) {
                char c = text[i]; if (c < 32 || c > 127) c = '?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
                kos::gfx::Compositor::DrawGlyph8x8(textX + i * 8u + 1u, y + 1u, glyph, kShadow, 0);
                kos::gfx::Compositor::DrawGlyph8x8(textX + i * 8u, y, glyph, rowColor, 0);
            }
            continue;
        }
        uint32_t labelLen = static_cast<uint32_t>(split - text);
        uint32_t valueLen = 0; while (split[1 + valueLen]) ++valueLen;
        if (labelLen > maxLabelChars) labelLen = maxLabelChars;
        if (valueLen > maxValueChars) valueLen = maxValueChars;
        for (uint32_t i = 0; i < labelLen; ++i) {
            char c = text[i]; if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(textX + i * 8u + 1u, y + 1u, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(textX + i * 8u, y, glyph, kLabel, 0);
        }
        for (uint32_t i = 0; i < valueLen; ++i) {
            char c = split[1 + i]; if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(valueX + i * 8u + 1u, y + 1u, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(valueX + i * 8u, y, glyph, kFg, 0);
        }
    }
}

bool HardwareInfoComponent::OnInputEvent(const input::InputEvent& event) {
    if (event.type == input::EventType::MousePress) {
        bool handled = HandleTabClick(event.mouse_data.x, event.mouse_data.y);
        handled = HandlePciPagerClick(event.mouse_data.x, event.mouse_data.y) || handled;
        return handled;
    }
    return false;
}

void HardwareInfoComponent::OnWindowResized(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    refresh_ticks_ = 0;
    InvalidateContent();
}

// ============================================================================
// FileBrowser Component
// ============================================================================

const FileBrowserComponent::Bookmark FileBrowserComponent::kBookmarks[kBookmarkCount] = {
    { "Root /", "/" },
    { "/bin", "/bin" },
    { "/etc", "/etc" },
    { "/var", "/var" },
    { "/var/log", "/var/log" },
};

bool FileBrowserComponent::EnumDirCallback(const kos::fs::DirEntry* entry, void* userdata) {
    if (!userdata || !entry) return false;
    FileBrowserComponent* self = reinterpret_cast<FileBrowserComponent*>(userdata);
    if (self->entry_count_ < 64u) {
        self->entries_[self->entry_count_++] = *entry;
    }
    return self->entry_count_ < 64u;
}

uint32_t FileBrowserComponent::StrLen(const char* s) const {
    uint32_t n = 0;
    while (s[n]) ++n;
    return n;
}

void FileBrowserComponent::StrCopy(char* dst, uint32_t dstSize, const char* src) const {
    uint32_t i = 0;
    while (src[i] && i + 1u < dstSize) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

void FileBrowserComponent::GoUp() {
    uint32_t len = StrLen(current_path_);
    if (len <= 1u) return;
    int32_t i = static_cast<int32_t>(len) - 1;
    while (i > 0 && current_path_[i] != '/') --i;
    if (i == 0) current_path_[1] = 0;
    else current_path_[i] = 0;
    needs_refresh_ = true;
    page_ = 0;
    selected_ = -1;
}

void FileBrowserComponent::NavigateTo(const char* path) {
    StrCopy(current_path_, sizeof(current_path_), path);
    needs_refresh_ = true;
    page_ = 0;
    selected_ = -1;
}

void FileBrowserComponent::NavigateInto(const int8_t* entryName) {
    uint32_t baseLen = StrLen(current_path_);
    char newPath[128];
    for (uint32_t i = 0; i < baseLen && i < 127u; ++i) newPath[i] = current_path_[i];
    uint32_t p = baseLen;
    if (p > 0 && newPath[p - 1] != '/' && p + 1u < 128u) newPath[p++] = '/';
    for (uint32_t i = 0; entryName[i] && p + 1u < 128u; ++i) newPath[p++] = static_cast<char>(entryName[i]);
    newPath[p] = 0;
    NavigateTo(newPath);
}

void FileBrowserComponent::RefreshEntries() {
    entry_count_ = 0;
    if (!kos::fs::g_fs_ptr) { needs_refresh_ = false; return; }
    kos::fs::g_fs_ptr->EnumDir(reinterpret_cast<const int8_t*>(current_path_), EnumDirCallback, this);
    needs_refresh_ = false;
}

bool FileBrowserComponent::HandleMouseClick(int mx, int my) {
    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != GetWindowId()) return false;

    auto inRect = [](int px, int py, const kos::gfx::Rect& r) -> bool {
        return (px >= static_cast<int>(r.x) && px < static_cast<int>(r.x + r.w) &&
                py >= static_cast<int>(r.y) && py < static_cast<int>(r.y + r.h));
    };

    if (inRect(mx, my, up_btn_)) {
        GoUp();
        RefreshEntries();
        return true;
    }

    for (uint32_t bi = 0; bi < kBookmarkCount; ++bi) {
        if (inRect(mx, my, sidebar_btns_[bi])) {
            NavigateTo(kBookmarks[bi].path);
            RefreshEntries();
            return true;
        }
    }

    if (pager_visible_) {
        uint32_t pc = (entry_count_ + kPageSize - 1u) / kPageSize;
        if (pc == 0u) pc = 1u;
        if (inRect(mx, my, prev_btn_) && page_ > 0u) {
            --page_;
            selected_ = -1;
            return true;
        }
        if (inRect(mx, my, next_btn_) && page_ + 1u < pc) {
            ++page_;
            selected_ = -1;
            return true;
        }
    }

    const uint32_t eStart = page_ * kPageSize;
    for (uint32_t ri = 0; ri < visible_count_; ++ri) {
        if (item_rects_[ri].w == 0) continue;
        if (inRect(mx, my, item_rects_[ri])) {
            const uint32_t ei = eStart + ri;
            if (selected_ == static_cast<int32_t>(ei)) {
                if (entries_[ei].isDir) {
                    NavigateInto(entries_[ei].name);
                    RefreshEntries();
                }
                selected_ = -1;
            } else {
                selected_ = static_cast<int32_t>(ei);
            }
            return true;
        }
    }
    return false;
}

void FileBrowserComponent::Render() {
    const uint32_t wid = GetWindowId();
    if (!wid) return;

    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(wid, d)) return;

    if (needs_refresh_) RefreshEntries();

    constexpr uint32_t kBgA      = 0xFF0F1320u;
    constexpr uint32_t kBgB      = 0xFF121728u;
    constexpr uint32_t kHdrBg    = 0xFF141C30u;
    constexpr uint32_t kSideBg   = 0xFF0C1022u;
    constexpr uint32_t kSideAct  = 0xFF1E3158u;
    constexpr uint32_t kPanel    = 0xFF111928u;
    constexpr uint32_t kSelBg    = 0xFF1A3A6Au;
    constexpr uint32_t kBorderHi = 0xFF4A6A9Au;
    constexpr uint32_t kBorderLo = 0xFF1E2E50u;
    constexpr uint32_t kFgDir    = 0xFFFFB84Du;
    constexpr uint32_t kFgFile   = 0xFFB8CDE8u;
    constexpr uint32_t kFgPath   = 0xFF7AAED8u;
    constexpr uint32_t kFgText   = 0xFFE0EAFFu;
    constexpr uint32_t kShadow   = 0xFF04070Fu;
    constexpr uint32_t kBtn      = 0xFF1B3258u;

    const uint32_t flags = kos::ui::GetWindowFlags(wid);
    const bool frameless = (flags & kos::ui::WF_Frameless) != 0u;
    const uint32_t cx = d.x + 1u;
    const uint32_t cy = d.y + (frameless ? 1u : (kos::ui::TitleBarHeight() + 1u));
    const uint32_t cw = (d.w > 2u) ? d.w - 2u : d.w;
    const uint32_t ch = frameless
        ? ((d.h > 2u) ? d.h - 2u : d.h)
        : ((d.h > kos::ui::TitleBarHeight() + 2u) ? (d.h - kos::ui::TitleBarHeight() - 2u) : 0u);
    if (cw < 120u || ch < 80u) return;

    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);
    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderLo);

    const uint32_t hdrX = cx + 1u;
    const uint32_t hdrY = cy + 2u;
    const uint32_t hdrW = (cw > 2u) ? cw - 2u : cw;
    kos::gfx::Compositor::FillRect(hdrX, hdrY, hdrW, kHeaderHeight, kHdrBg);
    kos::gfx::Compositor::FillRect(hdrX, hdrY + kHeaderHeight, hdrW, 1, kBorderLo);

    const uint32_t upBtnW = 24u, upBtnH = kHeaderHeight - 2u;
    const uint32_t upBtnX = hdrX + 2u, upBtnY = hdrY + 1u;
    up_btn_ = {upBtnX, upBtnY, upBtnW, upBtnH};
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY, upBtnW, upBtnH, kBtn);
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY, upBtnW, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY + upBtnH - 1, upBtnW, 1, kBorderLo);
    {
        const char* lbl = "^ Up";
        for (uint32_t i = 0; lbl[i]; ++i) {
            kos::gfx::Compositor::DrawGlyph8x8(upBtnX + 2u + i * 8u, upBtnY + 1u,
                kos::gfx::kFont8x8Basic[lbl[i] - 32], kFgText, 0);
        }
    }

    const uint32_t pathX = upBtnX + upBtnW + 4u;
    const uint32_t pathY = hdrY + 4u;
    const uint32_t pathMaxChars = (hdrX + hdrW > pathX + 8u) ? ((hdrX + hdrW - pathX) / 8u) : 0u;
    for (uint32_t i = 0; current_path_[i] && i < pathMaxChars; ++i) {
        char c = current_path_[i]; if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
        kos::gfx::Compositor::DrawGlyph8x8(pathX + i * 8u + 1u, pathY + 1u, glyph, kShadow, 0);
        kos::gfx::Compositor::DrawGlyph8x8(pathX + i * 8u, pathY, glyph, kFgPath, 0);
    }

    const uint32_t bodyY = hdrY + kHeaderHeight + 1u;
    const uint32_t bodyH = (cy + ch > bodyY + 2u) ? (cy + ch - bodyY - 2u) : 0u;
    if (bodyH == 0u) return;

    const uint32_t sideW = (cw > kSidebarWidth + 60u) ? kSidebarWidth : (cw / 4u);
    const uint32_t sideX = cx + 1u;
    kos::gfx::Compositor::FillRect(sideX, bodyY, sideW, bodyH, kSideBg);
    kos::gfx::Compositor::FillRect(sideX + sideW, bodyY, 1, bodyH, kBorderLo);

    {
        const char* lbl = "PLACES";
        for (uint32_t i = 0; lbl[i]; ++i) {
            kos::gfx::Compositor::DrawGlyph8x8(sideX + 4u + i * 8u, bodyY + 3u,
                kos::gfx::kFont8x8Basic[lbl[i] - 32], kBorderHi, 0);
        }
    }

    const uint32_t sideItemY = bodyY + 13u;
    const uint32_t sideMaxChars = (sideW > 8u) ? (sideW - 8u) / 8u : 0u;
    for (uint32_t bi = 0; bi < kBookmarkCount; ++bi) {
        const uint32_t bY = sideItemY + bi * (kRowHeight + 2u);
        sidebar_btns_[bi] = {sideX, bY, sideW, kRowHeight + 2u};
        const char* bp = kBookmarks[bi].path;
        uint32_t bpLen = StrLen(bp), fpLen = StrLen(current_path_);
        bool active = (fpLen == bpLen);
        if (active) {
            for (uint32_t ci = 0; ci < fpLen; ++ci) {
                if (current_path_[ci] != bp[ci]) { active = false; break; }
            }
        }
        if (active) {
            kos::gfx::Compositor::FillRect(sideX, bY, sideW, kRowHeight + 2u, kSideAct);
            kos::gfx::Compositor::FillRect(sideX, bY, 2u, kRowHeight + 2u, kFgDir);
        }
        const char* lbl = kBookmarks[bi].label;
        for (uint32_t i = 0; lbl[i] && i < sideMaxChars; ++i) {
            kos::gfx::Compositor::DrawGlyph8x8(sideX + 6u + i * 8u, bY + 2u,
                kos::gfx::kFont8x8Basic[lbl[i] - 32], active ? kFgText : kFgPath, 0);
        }
    }

    const uint32_t mainX = sideX + sideW + 1u;
    const uint32_t mainW = (cx + cw > mainX + 4u) ? (cx + cw - mainX - 2u) : 0u;
    if (mainW < 20u) return;
    FillCheckerRect(mainX, bodyY, mainW, bodyH, kPanel, 0xFF0E1828u, 2);
    kos::gfx::Compositor::FillRect(mainX, bodyY, mainW, 1, kBorderHi);

    pager_visible_ = false;
    prev_btn_ = {0, 0, 0, 0};
    next_btn_ = {0, 0, 0, 0};
    uint32_t footerH = 0u;
    uint32_t pageCount = (entry_count_ + kPageSize - 1u) / kPageSize;
    if (pageCount == 0u) pageCount = 1u;
    if (page_ >= pageCount) page_ = pageCount - 1u;
    if (pageCount > 1u) {
        footerH = 14u;
        constexpr uint32_t btnW = 50u, btnH = 12u;
        const uint32_t by = (bodyY + bodyH > btnH + 2u) ? (bodyY + bodyH - btnH - 2u) : bodyY;
        const uint32_t nextX = (mainX + mainW > btnW + 6u) ? (mainX + mainW - btnW - 4u) : mainX;
        const uint32_t prevX = (nextX > btnW + 6u) ? (nextX - btnW - 4u) : mainX;
        prev_btn_ = {prevX, by, btnW, btnH};
        next_btn_ = {nextX, by, btnW, btnH};
        pager_visible_ = true;
        FillCheckerRect(prevX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        FillCheckerRect(nextX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        kos::gfx::Compositor::FillRect(prevX, by, btnW, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(nextX, by, btnW, 1, kBorderHi);
        const char* pl = "< Prev";
        const char* nl = "Next >";
        for (uint32_t i = 0; pl[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(prevX + 3u + i * 8u, by + 2u,
                kos::gfx::kFont8x8Basic[pl[i] - 32], kFgText, 0);
        for (uint32_t i = 0; nl[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(nextX + 3u + i * 8u, by + 2u,
                kos::gfx::kFont8x8Basic[nl[i] - 32], kFgText, 0);

        char pgBuf[16]; uint32_t pp = 0;
        BufPutDec(pgBuf, sizeof(pgBuf), pp, page_ + 1u);
        BufPutChar(pgBuf, sizeof(pgBuf), pp, '/');
        BufPutDec(pgBuf, sizeof(pgBuf), pp, pageCount);
        pgBuf[pp] = 0;
        uint32_t pgIndX = (prevX > pp * 8u + 8u) ? (prevX - pp * 8u - 8u) : mainX;
        for (uint32_t i = 0; pgBuf[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(pgIndX + i * 8u, by + 2u,
                kos::gfx::kFont8x8Basic[pgBuf[i] - 32], kFgPath, 0);
    }

    const uint32_t listY = bodyY + 2u;
    const uint32_t listH = (bodyH > footerH + 4u) ? (bodyH - footerH - 4u) : 0u;
    uint32_t maxVisRows = listH / kRowHeight;
    if (maxVisRows > kPageSize) maxVisRows = kPageSize;
    const uint32_t eStart = page_ * kPageSize;
    const uint32_t eEnd = (eStart + maxVisRows < entry_count_) ? (eStart + maxVisRows) : entry_count_;
    visible_count_ = eEnd - eStart;

    const uint32_t kSizeW = 56u;
    const uint32_t maxNameCh = (mainW > kSizeW + 30u) ? ((mainW - kSizeW - 30u) / 8u) : 0u;

    for (uint32_t ri = 0; ri < maxVisRows; ++ri) {
        const uint32_t ei = eStart + ri;
        const uint32_t ry = listY + ri * kRowHeight;
        if (ei >= entry_count_) {
            item_rects_[ri] = {0, 0, 0, 0};
            continue;
        }
        const bool isSelected = (selected_ == static_cast<int32_t>(ei));
        const bool isDir = entries_[ei].isDir;
        const uint32_t rowBg = isSelected ? kSelBg : (isDir ? 0xFF141E35u : 0xFF0F1928u);
        kos::gfx::Compositor::FillRect(mainX, ry, mainW, kRowHeight, rowBg);
        kos::gfx::Compositor::FillRect(mainX, ry + kRowHeight - 1, mainW, 1, 0xFF1A2640u);
        item_rects_[ri] = {mainX, ry, mainW, kRowHeight};

        const char* iconStr = isDir ? "[>]" : "[ ]";
        const uint32_t iconC = isDir ? kFgDir : 0xFF4A6A9Au;
        for (uint32_t i = 0; iconStr[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 2u + i * 8u, ry + 2u,
                kos::gfx::kFont8x8Basic[iconStr[i] - 32], iconC, 0);

        const int8_t* name = entries_[ei].name;
        const uint32_t fgCol = isDir ? kFgDir : kFgFile;
        for (uint32_t i = 0; name[i] && i < maxNameCh; ++i) {
            char c = static_cast<char>(name[i]);
            if (c < 32 || c > 127) continue;
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 28u + i * 8u + 1u, ry + 3u, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 28u + i * 8u, ry + 2u, glyph, fgCol, 0);
        }

        if (!isDir && entries_[ei].size > 0) {
            char szBuf[12]; uint32_t sp = 0;
            const uint32_t sz = entries_[ei].size;
            if (sz >= 1024u * 1024u) {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz / (1024u * 1024u));
                BufPutStr(szBuf, sizeof(szBuf), sp, "MB");
            } else if (sz >= 1024u) {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz / 1024u);
                BufPutChar(szBuf, sizeof(szBuf), sp, 'K');
            } else {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz);
                BufPutChar(szBuf, sizeof(szBuf), sp, 'B');
            }
            szBuf[sp] = 0;
            const uint32_t szTW = sp * 8u;
            const uint32_t szX = (mainX + mainW > szTW + 4u) ? (mainX + mainW - szTW - 4u) : (mainX + mainW);
            for (uint32_t i = 0; szBuf[i]; ++i) {
                kos::gfx::Compositor::DrawGlyph8x8(szX + i * 8u, ry + 2u,
                    kos::gfx::kFont8x8Basic[szBuf[i] - 32], 0xFF6A8AA8u, 0);
            }
        }
    }

    if (!kos::fs::g_fs_ptr || entry_count_ == 0) {
        const char* msg = kos::fs::g_fs_ptr ? "Empty directory" : "No filesystem mounted";
        for (uint32_t i = 0; msg[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 8u + i * 8u, listY + 4u,
                kos::gfx::kFont8x8Basic[msg[i] - 32], 0xFF8AA0C0u, 0);
    }

    char infoBuf[32]; uint32_t ip = 0;
    BufPutDec(infoBuf, sizeof(infoBuf), ip, entry_count_);
    BufPutStr(infoBuf, sizeof(infoBuf), ip, " item(s)");
    infoBuf[ip] = 0;
    const uint32_t infoY = (bodyY + bodyH > 10u) ? (bodyY + bodyH - 10u) : bodyY;
    for (uint32_t i = 0; infoBuf[i]; ++i)
        kos::gfx::Compositor::DrawGlyph8x8(mainX + 4u + i * 8u, infoY,
            kos::gfx::kFont8x8Basic[infoBuf[i] - 32], kBorderHi, 0);
}

bool FileBrowserComponent::OnInputEvent(const input::InputEvent& event) {
    if (event.type == input::EventType::MousePress) {
        return HandleMouseClick(event.mouse_data.x, event.mouse_data.y);
    }
    return false;
}

void FileBrowserComponent::OnWindowResized(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    needs_refresh_ = true;
    InvalidateContent();
}

// ============================================================================
// Process Monitor Component
// ============================================================================

void ProcessMonitorComponent::Render() {
    kos::ui::ProcessViewer::Render();
}

bool ProcessMonitorComponent::OnInputEvent(const input::InputEvent& event) {
    if (event.type == input::EventType::KeyPress) {
        kos::ui::ProcessViewer::OnKeyDown(static_cast<int8_t>(event.key_data.key_code & 0xFFu));
        InvalidateContent();
        return true;
    }
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
