#include <graphics/terminal.hpp>
#include <graphics/font8x8_basic.hpp>
#include <graphics/font8x16_basic.hpp>
#include <ui/framework.hpp>
#include <lib/string.hpp>
#include <memory/heap.hpp>

using namespace kos::gfx;
using namespace kos::ui;
using namespace kos::lib;
using namespace kos::memory;

namespace kos { namespace gfx {

Terminal::Cell* Terminal::s_buffer = nullptr;
uint32_t Terminal::s_cols = 0;
uint32_t Terminal::s_rows = 0;
uint32_t Terminal::s_cursorCol = 0;
uint32_t Terminal::s_cursorRow = 0;
uint8_t Terminal::s_fg = 7; // light gray default
uint8_t Terminal::s_bg = 0; // black
uint32_t Terminal::s_windowId = 0;
bool Terminal::s_ready = false;
bool Terminal::s_tallFont = true;
uint32_t Terminal::s_scrollOffset = 0;
uint32_t Terminal::s_capacityRows = 0;

// Some 8x16 bitmap sources encode leftmost pixel in MSB (bit7). Our rasterizer expects LSB-left.
// Set this flag according to the font data orientation.
static constexpr bool kFont8x16IsLSBLeft = false; // current kFont8x16Basic values are MSB-left style

static inline uint8_t reverseBits8(uint8_t v) {
    // Bit-reverse 8-bit value (01234567 -> 76543210)
    v = (uint8_t)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (uint8_t)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (uint8_t)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

static inline void DrawGlyph8x8WithOrientation(uint32_t gx, uint32_t gy, const uint8_t* glyph8, uint32_t fg, uint32_t bg, bool isLSBLeft) {
    if (isLSBLeft) {
        kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph8, fg, bg);
    } else {
        uint8_t tmp[8];
        for (int r = 0; r < 8; ++r) tmp[r] = reverseBits8(glyph8[r]);
        kos::gfx::Compositor::DrawGlyph8x8(gx, gy, tmp, fg, bg);
    }
}

static inline uint32_t vgaColorToRGB(uint8_t c) {
    // Simple 16-color palette mapping (rough approximations)
    switch (c & 0x0F) {
        case 0: return 0xFF000000u; // black
        case 1: return 0xFF0000AAu; // blue
        case 2: return 0xFF00AA00u; // green
        case 3: return 0xFF00AAAAu; // cyan
        case 4: return 0xFFAA0000u; // red
        case 5: return 0xFFAA00AAu; // magenta
        case 6: return 0xFFAA5500u; // brown/dk yellow
        case 7: return 0xFFAAAAAAu; // light gray
        case 8: return 0xFF555555u; // dark gray
        case 9: return 0xFF5555FFu; // light blue
        case 10: return 0xFF55FF55u; // light green
        case 11: return 0xFF55FFFFu; // light cyan
        case 12: return 0xFFFF5555u; // light red
        case 13: return 0xFFFF55FFu; // light magenta
        case 14: return 0xFFFFFF55u; // yellow
        case 15: return 0xFFFFFFFFu; // white
    }
    return 0xFF000000u;
}

bool Terminal::Initialize(uint32_t winId, uint32_t cols, uint32_t rows, bool useTallFont) {
    if (s_ready) return true;
    s_cols = cols; s_rows = rows;
    s_tallFont = useTallFont;
    s_capacityRows = rows * 8; // allow up to 8x visible rows in scrollback
    s_scrollOffset = 0;
    uint32_t count = cols * s_capacityRows;
    s_buffer = (Cell*)Heap::Alloc(count * sizeof(Cell));
    if (!s_buffer) return false;
    for (uint32_t i=0;i<count;++i) { s_buffer[i].ch=' '; s_buffer[i].fg=s_fg; s_buffer[i].bg=s_bg; }
    s_cursorCol = 0; s_cursorRow = 0; s_windowId = winId;
    s_ready = true;
    return true;
}

void Terminal::Shutdown() {
    s_ready = false; s_buffer = nullptr;
}

bool Terminal::IsActive() { return s_ready && s_windowId != 0; }

void Terminal::SetWindow(uint32_t wid) { s_windowId = wid; }

uint32_t Terminal::GetWindowId() { return s_windowId; }

// Runtime tall font toggle API (simple): adjust flag via public method; caller may Clear() for visual reset
extern "C" void Terminal_SetTallFont(int enable) {
    Terminal::SetTallFont(enable != 0);
}

void Terminal::SetTallFont(bool enable) { s_tallFont = enable; }

void Terminal::Clear() {
    if (!IsActive()) return;
    uint32_t count = s_cols * s_capacityRows;
    for (uint32_t i=0;i<count;++i) { s_buffer[i].ch=' '; s_buffer[i].fg=s_fg; s_buffer[i].bg=s_bg; }
    s_cursorCol = 0; s_cursorRow = 0; s_scrollOffset = 0;
}

void Terminal::SetColor(uint8_t fg, uint8_t bg) {
    s_fg = (fg & 0x0F); s_bg = (bg & 0x0F);
}

void Terminal::MoveCursor(uint32_t col, uint32_t row) {
    if (!IsActive()) return;
    if (col >= s_cols) col = s_cols - 1;
    if (row >= s_capacityRows) row = s_capacityRows - 1;
    s_cursorCol = col;
    s_cursorRow = row;
}

void Terminal::GetSize(uint32_t& outCols, uint32_t& outRows) {
    outCols = s_cols;
    outRows = s_rows;
}

void Terminal::advanceCursor() {
    ++s_cursorCol;
    if (s_cursorCol >= s_cols) { s_cursorCol = 0; ++s_cursorRow; }
    scrollIfNeeded();
}

void Terminal::scrollIfNeeded() {
    if (s_cursorRow < s_capacityRows) return;
    // Move cursor back to last buffer row allowing continuous append
    s_cursorRow = s_capacityRows - 1;
    // Shift all rows up by one for infinite scroll style
    for (uint32_t r=1; r<s_capacityRows; ++r) {
        Cell* dst = &s_buffer[(r-1)*s_cols];
        Cell* src = &s_buffer[r*s_cols];
        for (uint32_t c=0;c<s_cols;++c) dst[c] = src[c];
    }
    // Clear last row
    Cell* last = &s_buffer[(s_capacityRows-1)*s_cols];
    for (uint32_t c=0;c<s_cols;++c) { last[c].ch=' '; last[c].fg=s_fg; last[c].bg=s_bg; }
}

void Terminal::PutChar(int8_t ch) {
    if (!IsActive()) return;
    if (ch == '\n') {
        s_cursorCol = 0; ++s_cursorRow; scrollIfNeeded(); return;
    } else if (ch == '\r') {
        s_cursorCol = 0; return;
    } else if (ch == '\t') {
        for (int i=0;i<4;++i) PutChar(' '); return;
    } else if (ch == '\b') {
        if (s_cursorCol > 0) { --s_cursorCol; }
        return;
    }
    if (s_cursorRow >= s_capacityRows) scrollIfNeeded();
    uint32_t idx = s_cursorRow * s_cols + s_cursorCol;
    if (idx < s_cols * s_capacityRows) {
        s_buffer[idx].ch = (char)ch;
        s_buffer[idx].fg = s_fg;
        s_buffer[idx].bg = s_bg;
    }
    advanceCursor();
}

void Terminal::Write(const int8_t* s) {
    if (!IsActive() || !s) return;
    while (*s) { PutChar(*s++); }
}

void Terminal::Render() {
    if (!IsActive()) return;
    // Obtain window rectangle
    kos::gfx::WindowDesc desc; if (!kos::ui::GetWindowDesc(s_windowId, desc)) return;
    // Compute drawable client area (skip title bar ~18px)
    uint32_t clientX = desc.x;
    uint32_t clientY = desc.y + 18; // title bar
    uint32_t clientW = desc.w;
    uint32_t clientH = (desc.h > 18) ? (desc.h - 18) : 0;
    if (clientH == 0) return;
    // Character cell size (8x8)
    const uint32_t cw = 8; const uint32_t ch = s_tallFont ? 16u : 8u;
    uint32_t maxRows = clientH / ch;
    uint32_t maxCols = clientW / cw;
    if (maxRows == 0 || maxCols == 0) return;
    // Limit rendering to min(s_rows, maxRows) etc.
    uint32_t visibleRows = (s_rows < maxRows ? s_rows : maxRows);
    // Determine highest starting row considering scrollOffset
    uint32_t startRow = s_scrollOffset;
    if (startRow + visibleRows > s_capacityRows) {
        if (visibleRows <= s_capacityRows) startRow = s_capacityRows - visibleRows;
        else startRow = 0;
    }
    uint32_t cols = (s_cols < maxCols ? s_cols : maxCols);
    // Draw glyphs using compositor backbuffer API
    for (uint32_t r=0; r<visibleRows; ++r) {
        uint32_t bufferRow = startRow + r;
        for (uint32_t c=0; c<cols; ++c) {
            const Cell& cell = s_buffer[bufferRow * s_cols + c];
            uint32_t fgRGB = vgaColorToRGB(cell.fg);
            uint32_t bgRGB = vgaColorToRGB(cell.bg);
            char chv = cell.ch;
            if (chv < 32 || chv > 127) chv = '?';
            // Use 8x8 base font for both modes; for tall mode, we draw it twice vertically to form 8x16.
            const uint8_t* glyph8 = kFont8x8Basic[chv - 32];
            uint32_t gx = clientX + c * cw;
            uint32_t gy = clientY + r * ch;
            if (s_tallFont) {
                // Draw 8x8 glyph twice vertically to synthesize 8x16
                kos::gfx::Compositor::DrawGlyph8x8(gx, gy,      glyph8, fgRGB, bgRGB);
                kos::gfx::Compositor::DrawGlyph8x8(gx, gy + 8,  glyph8, fgRGB, bgRGB);
            } else {
                kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph8, fgRGB, bgRGB);
            }
        }
    }
    // Cursor: draw as inverse block by swapping fg/bg on a space glyph
    if (s_cursorRow >= startRow && s_cursorRow < startRow + visibleRows && s_cursorCol < cols) {
        uint32_t gx = clientX + s_cursorCol * cw;
        uint32_t gy = clientY + (s_cursorRow - startRow) * ch;
        uint32_t fg = vgaColorToRGB(s_bg);
        uint32_t bg = vgaColorToRGB(s_fg);
        static const uint8_t space[8] = {0,0,0,0,0,0,0,0};
        if (s_tallFont) {
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy,     space, fg, bg);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy + 8, space, fg, bg);
        } else {
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, space, fg, bg);
        }
    }
}

void Terminal::ScrollPageUp() {
    if (s_scrollOffset > 0) {
        uint32_t delta = s_rows; // page size = visible rows
        if (delta > s_scrollOffset) delta = s_scrollOffset;
        s_scrollOffset -= delta;
    }
}

void Terminal::ScrollPageDown() {
    uint32_t maxStart = (s_capacityRows > s_rows ? s_capacityRows - s_rows : 0);
    if (s_scrollOffset < maxStart) {
        uint32_t delta = s_rows;
        uint32_t remaining = maxStart - s_scrollOffset;
        if (delta > remaining) delta = remaining;
        s_scrollOffset += delta;
    }
}

}} // namespace
