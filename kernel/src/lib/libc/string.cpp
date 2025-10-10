// Pure C implementations of minimal string functions exposed to applications.
// Kept in a C++ file for build consistency, but functions are C ABI and contain only C code.
extern "C" {

    #include <lib/libc/stdint.h>

    int strcmp(const int8_t* a, const int8_t* b) {
        const uint8_t* pa = (const uint8_t*)a;
        const uint8_t* pb = (const uint8_t*)b;
        while (*pa && (*pa == *pb)) {
            ++pa; ++pb;
        }
        return (int)(unsigned int)(*pa) - (int)(unsigned int)(*pb);
    }

    int strncmp(const int8_t* a, const int8_t* b, size_t len) {
        const uint8_t* pa = (const uint8_t*)a;
        const uint8_t* pb = (const uint8_t*)b;
        for (size_t i = 0; i < len; ++i) {
            uint8_t ca = pa[i];
            uint8_t cb = pb[i];
            if (ca != cb || ca == 0 || cb == 0) {
                return (int)ca - (int)cb;
            }
        }
        return 0;
    }

    size_t strlen(const int8_t* s) {
        size_t len = 0;
        while (*s++) { ++len; }
        return len;
    }
}