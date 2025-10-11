#include <lib/libc/stdint.h>
#include <lib/libc/stdio.h>
#include <lib/libc/string.h>

static int8_t hex_digit(uint8_t v) {
    v &= 0xF;
    return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

static void dump(const int8_t* label, const uint8_t* buf, uint32_t n) {
    kos_printf((int8_t*)"%s:", label);
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t b = buf[i];
        int8_t out[3];
        out[0] = hex_digit(b >> 4);
        out[1] = hex_digit(b);
        out[2] = 0;
        kos_printf((int8_t*)" %s", out);
    }
    kos_printf((int8_t*)"\n");
}

void app_memtest(void) {
    uint8_t src[8] = {0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
    uint8_t dst[8] = {0};

    memcpy(dst, src, 8);

    int ok = 1;
    for (int i = 0; i < 8; ++i) if (dst[i] != src[i]) ok = 0;

    dump((int8_t*)"src", src, 8);
    dump((int8_t*)"dst", dst, 8);
    kos_printf((int8_t*)"memcpy %s\n", ok ? (int8_t*)"OK" : (int8_t*)"FAIL");
}

#ifndef APP_EMBED
int main(void) {
    app_memtest();
    return 0;
}
#endif
