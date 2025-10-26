#ifndef KOS_LIB_STRING_HPP
#define KOS_LIB_STRING_HPP

#include <lib/libc/stdint.h>

namespace kos { 
    namespace lib {
        class String {
            public:
            typedef unsigned int size_t;
                static int32_t strcmp(const uint8_t* a, const uint8_t* b);
                static int32_t strcmp(int8_t* a, const int8_t* b, uint32_t len);
                static int32_t strcmp(const int8_t* a, const int8_t* b, uint32_t len);
                static int8_t* strncpy(int8_t* dest, const int8_t* src, uint32_t n);
                static uint32_t strlen(const int8_t* s);
                // Memory movement that handles overlap (like C's memmove)
                static void* memmove(void* dest, const void* src, uint32_t n);
                // Scan memory for a byte value (like C's memchr)
                static void* memchr(const void* s, int c, uint32_t n);
                // Compare memory blocks (like C's memcmp)
                static int32_t memcmp(const void* a, const void* b, uint32_t n);
                // Set memory to a byte value (like C's memset)
                static void* memset(void* s, int c, uint32_t n);
        };
    }
}

#endif
