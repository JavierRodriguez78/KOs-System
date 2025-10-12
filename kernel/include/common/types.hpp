#ifndef __KOS__COMMON__TYPES_H
#define __KOS__COMMON__TYPES_H

namespace kos
{
   namespace common
   { 
        typedef char                    int8_t;
        typedef unsigned char           uint8_t;
        typedef short                   int16_t;
        typedef unsigned short          uint16_t;
        typedef int                     int32_t;
        typedef unsigned                uint32_t;
        typedef long long int           int64_t;
        typedef unsigned long long int  uint64_t;

      // Pointer-sized unsigned integer
      // Select width based on common architecture macros (defaults to 32-bit)
      #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__ppc64__) || defined(__mips64)
         typedef uint64_t            uintptr_t;
      #else
         typedef uint32_t            uintptr_t;
      #endif
   }
}


#endif