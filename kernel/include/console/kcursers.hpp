#pragma once
#include <common/types.hpp>
#include <console/tty.hpp>
#include <drivers/vga/vga.hpp>

namespace kos { namespace console { namespace kcursers {

// Minimal kcursers-like API for kernel terminal
// Functions are intentionally simple and synchronous.

inline void init() {
    TTY::Clear();
}

inline void end() { /* no teardown needed */ }

inline void clear() { TTY::Clear(); }

inline void refresh() { /* no-op for text mode */ }

inline void set_color(uint8_t fg, uint8_t bg) { TTY::SetColor(fg, bg); }

inline void move(uint32_t col, uint32_t row) { TTY::MoveCursor(col, row); }

inline void addch(char c) { TTY::PutChar((int8_t)c); }

inline void addstr(const char* s) { TTY::Write((const int8_t*)s); }

inline void getmaxyx(uint32_t& rows, uint32_t& cols) {
    uint8_t w, h; kos::drivers::vga::VGA::GetSize(w, h);
    cols = w; rows = h;
}

inline void draw_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (w < 2 || h < 2) return;
    move(x, y);
    addch('+'); for (uint32_t i=0;i<w-2;++i) addch('-'); addch('+');
    for (uint32_t r=1; r<h-1; ++r) {
        move(x, y+r);
        addch('|'); for (uint32_t i=0;i<w-2;++i) addch(' '); addch('|');
    }
    move(x, y+h-1);
    addch('+'); for (uint32_t i=0;i<w-2;++i) addch('-'); addch('+');
}

}}} // namespace kos::console::kcursers
