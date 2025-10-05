#ifndef  __KOS__LIB__LIBC_H
#define  __KOS__LIB__LIBC_H


#include <common/types.hpp>

namespace kos{
    namespace lib{
        class LibC{
            public:
                LibC();
                ~LibC();
                kos::common::int32_t strcmp(const kos::common::uint8_t* a, const kos::common::uint8_t* b);
                kos::common::int32_t strcmp(kos::common::int8_t* a, const kos::common::int8_t* b, kos::common::uint32_t len);
                kos::common::uint32_t strlen(const kos::common::int8_t* s);
        };
    }
}

#endif