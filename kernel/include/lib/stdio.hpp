#ifndef KOS_LIB_STDIO_HPP
#define KOS_LIB_STDIO_HPP

#include <stdarg.h>
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace sys {

        struct ApiTable {
            void (*putc)(int8_t c);
            void (*puts)(const int8_t* s);
            void (*hex)(uint8_t v);
            void (*listroot)();
        };

        // Access to the API table (placed by the kernel at a fixed address)
        ApiTable* table();

        // Non-inline wrappers
        void putc(int8_t c);
        void puts(const int8_t* s);
        void hex(uint8_t v);
        void listroot();

        // Minimal printf-like output
        void vprintf(const int8_t* fmt, va_list ap);
        void printf(const int8_t* fmt, ...);

    }
}
#endif