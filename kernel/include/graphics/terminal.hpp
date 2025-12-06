#ifndef KOS_GRAPHICS_TERMINAL_HPP
#define KOS_GRAPHICS_TERMINAL_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>

namespace kos { namespace gfx {

// Simple graphical terminal that mirrors VGA TTY output when active.
// Not a full emulator; stores characters & fg/bg colors.
class Terminal {
public:
    static bool Initialize(uint32_t winId, uint32_t cols = 80, uint32_t rows = 25, bool useTallFont = true);
    static void Shutdown();
    static void PutChar(int8_t c);
    static void Write(const int8_t* s);
    static void Clear();
    static void SetColor(uint8_t fg, uint8_t bg);
    static void Render(); // draw into window area (called each frame)
    static void SetWindow(uint32_t wid); // change window id (after creation)
    static void SetTallFont(bool enable); // runtime toggle (8x16 vs 8x8)
    static bool IsActive();
    static uint32_t GetWindowId();
    // Basic cursor/size helpers for console cursor operations
    static void MoveCursor(uint32_t col, uint32_t row);
    static void GetSize(uint32_t& outCols, uint32_t& outRows);
private:
    struct Cell { char ch; uint8_t fg; uint8_t bg; };
    static Cell* s_buffer;
    static uint32_t s_cols, s_rows;
    static uint32_t s_cursorCol, s_cursorRow;
    static uint8_t s_fg, s_bg;
    static uint32_t s_windowId;
    static bool s_ready;
    static bool s_tallFont; // 8x16 vs 8x8
    static uint32_t s_scrollOffset; // top row index for scrollback
    static uint32_t s_capacityRows; // total stored rows
    static void ensureCapacity();
    static void scrollIfNeeded();
    static void advanceCursor();
    static void pageDown();
    static void pageUp();
public:
    // Scroll control (exposed for future keybindings)
    static void ScrollPageUp();
    static void ScrollPageDown();
};

}} // namespace

#endif // KOS_GRAPHICS_TERMINAL_HPP
