#ifndef  __KOS__LIB__LIBC_H
#define  __KOS__LIB__LIBC_H

#include <common/types.hpp>

using namespace kos::common;

namespace kos{
    namespace lib{
        class LibC{
            public:
                static int32_t strcmp(const uint8_t* a, const uint8_t* b);
                static int32_t strcmp(int8_t* a, const int8_t* b, uint32_t len);
                uint32_t strlen(const int8_t* s);
        };
    }
}

#endif