#include <lib/string.hpp>

using namespace kos::lib;

// C ABI wrappers so that applications using <lib/libc/string.h> link correctly
extern "C" {

    #include <lib/libc/stdint.h>

    int strcmp(const int8_t* a, const int8_t* b) {
        const uint8_t* pa = reinterpret_cast<const uint8_t*>(a);
        const uint8_t* pb = reinterpret_cast<const uint8_t*>(b);
        return String::strcmp(pa, pb);
    }

    int strncmp(const int8_t* a, const int8_t* b, size_t len) {
        // size_t may be larger than uint32_t; clamp safely for our 32-bit env
        uint32_t n = (len > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)len;
        return String::strcmp(reinterpret_cast<const int8_t*>(a),
                                      reinterpret_cast<const int8_t*>(b),
                                      n);
    }

    size_t strlen(const int8_t* s) {
        return (size_t)String::strlen(reinterpret_cast<const int8_t*>(s));
    }
}