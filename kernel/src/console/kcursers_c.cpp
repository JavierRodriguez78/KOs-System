#include <console/kcursers.hpp>
#include <console/kcursers_c.h>

using namespace kos::console::kcursers;

extern "C" {

void kc_init(void) { init(); }
void kc_end(void) { end(); }
void kc_clear(void) { clear(); }
void kc_refresh(void) { refresh(); }
void kc_set_color(uint8_t fg, uint8_t bg) { set_color(fg, bg); }
void kc_move(uint32_t col, uint32_t row) { move(col, row); }
void kc_addch(char c) { addch(c); }
void kc_addstr(const char* s) { addstr(s); }
void kc_getmaxyx(uint32_t* rows, uint32_t* cols) {
    uint32_t r=0,c=0; getmaxyx(r,c); if (rows) *rows=r; if (cols) *cols=c;
}
void kc_draw_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h) { draw_box(x,y,w,h); }

}
