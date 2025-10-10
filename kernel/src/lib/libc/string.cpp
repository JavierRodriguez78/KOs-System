// C++ LibC class implementation and C ABI wrappers
#include <lib/libc.hpp>
#include <lib/libc/string.h>

using namespace kos::lib;
using namespace kos::common;


int32_t LibC::strcmp(const uint8_t* a, const uint8_t* b){
    while(*a && (*a == *b)){
        a++;
        b++;
    }
    return (int32_t)(uint8_t)(*a) - (int32_t)(uint8_t)(*b);
}

int32_t LibC::strcmp(int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

int32_t LibC::strcmp(const int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

uint32_t LibC::strlen(const int8_t* s){
    uint32_t len = 0;
    while(*s++){
        len++;
    }
    return len;
}

// C ABI wrappers so that applications using <lib/libc/string.h> link correctly
extern "C" {
    int strcmp(const char* a, const char* b) {
        const kos::common::uint8_t* pa = reinterpret_cast<const kos::common::uint8_t*>(a);
        const kos::common::uint8_t* pb = reinterpret_cast<const kos::common::uint8_t*>(b);
        return kos::lib::LibC::strcmp(pa, pb);
    }

    int strncmp(const char* a, const char* b, size_t len) {
        // size_t may be larger than uint32_t; clamp safely for our 32-bit env
        kos::common::uint32_t n = (len > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (kos::common::uint32_t)len;
        return kos::lib::LibC::strcmp(reinterpret_cast<const kos::common::int8_t*>(a),
                                      reinterpret_cast<const kos::common::int8_t*>(b),
                                      n);
    }

    size_t strlen(const char* s) {
        return (size_t)kos::lib::LibC::strlen(reinterpret_cast<const kos::common::int8_t*>(s));
    }
}