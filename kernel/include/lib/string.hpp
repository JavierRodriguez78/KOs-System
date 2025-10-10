#ifndef KOS_LIB_STRING_HPP
#define KOS_LIB_STRING_HPP

#include <lib/libc/stdint.h>

namespace kos { 
    namespace lib {
        class String {
            public:
            static int32_t strcmp(const uint8_t* a, const uint8_t* b);
            static int32_t strcmp(int8_t* a, const int8_t* b, uint32_t len);
            static int32_t strcmp(const int8_t* a, const int8_t* b, uint32_t len);
            static uint32_t strlen(const int8_t* s);
    };

}}

#endif
