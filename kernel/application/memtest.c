#include "app_log.h"
#include <lib/libc/stdint.h>
#include <lib/libc/stdio.h>
#include <lib/libc/string.h>

static int8_t hex_digit(uint8_t v) {
    v &= 0xF;
    return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

static void dump(const int8_t* label, const uint8_t* buf, uint32_t n) {
    app_log((int8_t*)"%s:", label);
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t b = buf[i];
        int8_t out[3];
        out[0] = hex_digit(b >> 4);
        out[1] = hex_digit(b);
        out[2] = 0;
        app_log((int8_t*)" %s", out);
    }
    app_log((int8_t*)"\n");
}

void app_memtest(void) {
    uint8_t src[8] = {0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
    uint8_t dst[8] = {0};

    memcpy(dst, src, 8);

    int ok = 1;
    for (int i = 0; i < 8; ++i) if (dst[i] != src[i]) ok = 0;

    dump((int8_t*)"src", src, 8);
    dump((int8_t*)"dst", dst, 8);
    app_log((int8_t*)"memcpy %s\n", ok ? (int8_t*)"OK" : (int8_t*)"FAIL");

    // Test memmove with overlap (forward overlapping region, dest > src)
    uint8_t ov1[8] = {1,2,3,4,5,6,7,8};
    memmove(ov1 + 2, ov1, 4); // move first 4 bytes to start at index 2
    uint8_t exp1[8] = {1,2,1,2,3,4,7,8};
    int ok1 = 1;
    for (int i = 0; i < 8; ++i) if (ov1[i] != exp1[i]) ok1 = 0;
    dump((int8_t*)"memmove ov1", ov1, 8);
    app_log((int8_t*)"memmove forward overlap %s\n", ok1 ? (int8_t*)"OK" : (int8_t*)"FAIL");

    // Test memmove with overlap (backward overlapping region, dest < src)
    uint8_t ov2[8] = {1,2,3,4,5,6,7,8};
    memmove(ov2, ov2 + 2, 4); // move 4 bytes starting at index 2 to index 0
    uint8_t exp2[8] = {3,4,5,6,5,6,7,8};
    int ok2 = 1;
    for (int i = 0; i < 8; ++i) if (ov2[i] != exp2[i]) ok2 = 0;
    dump((int8_t*)"memmove ov2", ov2, 8);
    app_log((int8_t*)"memmove backward overlap %s\n", ok2 ? (int8_t*)"OK" : (int8_t*)"FAIL");

    // Test memchr: find existing and non-existing byte
    const uint8_t buf[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xBB, 0xEE};
    void* hit1 = memchr(buf, 0xBB, 6);
    void* hit2 = memchr(buf, 0x99, 6);
    int ok3 = ((uint8_t*)hit1 == &buf[1]) && (hit2 == 0);
    app_log((int8_t*)"memchr %s\n", ok3 ? (int8_t*)"OK" : (int8_t*)"FAIL");

    // Test memcmp: equal, less-than, greater-than
    const uint8_t ma[5] = {1,2,3,4,5};
    const uint8_t mb[5] = {1,2,3,4,5};
    const uint8_t mc[5] = {1,2,3,4,6};
    const uint8_t md[5] = {1,2,3,3,9};
    int r_eq = memcmp(ma, mb, 5);
    int r_lt = memcmp(md, mc, 5); // differs at index 3: 3 - 4 < 0
    int r_gt = memcmp(mc, md, 5); // differs at index 3: 4 - 3 > 0
    int ok4 = (r_eq == 0) && (r_lt < 0) && (r_gt > 0);
    app_log((int8_t*)"memcmp %s\n", ok4 ? (int8_t*)"OK" : (int8_t*)"FAIL");

    // Test memset: set all bytes and partial region
    uint8_t zb[8];
    memset(zb, 0x00, 8);
    int ok5 = 1;
    for (int i = 0; i < 8; ++i) if (zb[i] != 0x00) ok5 = 0;
    uint8_t pb[8] = {0,1,2,3,4,5,6,7};
    memset(pb + 2, 0xAA, 3); // indices 2..4
    int ok6 = (pb[0]==0 && pb[1]==1 && pb[2]==0xAA && pb[3]==0xAA && pb[4]==0xAA && pb[5]==5 && pb[6]==6 && pb[7]==7);
    app_log((int8_t*)"memset %s\n", (ok5 && ok6) ? (int8_t*)"OK" : (int8_t*)"FAIL");
}

#ifndef APP_EMBED
int main(void) {
    app_memtest();
    return 0;
}
#endif
