#ifndef KOS_LIB_LIBC_HPP
#define KOS_LIB_LIBC_HPP

#include <common/types.hpp>

namespace kos { namespace lib {

    class LibC {
    public:
        static kos::common::int32_t strcmp(const kos::common::uint8_t* a, const kos::common::uint8_t* b);
        static kos::common::int32_t strcmp(kos::common::int8_t* a, const kos::common::int8_t* b, kos::common::uint32_t len);
        static kos::common::int32_t strcmp(const kos::common::int8_t* a, const kos::common::int8_t* b, kos::common::uint32_t len);
        static kos::common::uint32_t strlen(const kos::common::int8_t* s);
    };

}}

#endif
