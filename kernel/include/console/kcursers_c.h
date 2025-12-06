#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <lib/libc/stdint.h>

void kc_init(void);
void kc_end(void);
void kc_clear(void);
void kc_refresh(void);
void kc_set_color(uint8_t fg, uint8_t bg);
void kc_move(uint32_t col, uint32_t row);
void kc_addch(char c);
void kc_addstr(const char* s);
void kc_getmaxyx(uint32_t* rows, uint32_t* cols);
void kc_draw_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#ifdef __cplusplus
}
#endif
