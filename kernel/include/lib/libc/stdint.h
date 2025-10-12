#ifndef  LIBC__STDINT_H
#define  LIBC__STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

// Minimal C-compatible typedefs for freestanding environment
#if !defined(__stddef_size_t) && !defined(_SIZE_T) && !defined(_SIZE_T_DEFINED) && !defined(__size_t_defined)
typedef unsigned long size_t; // Works for i386; adjust if needed
#define __size_t_defined
#endif

// C standard-like prototypes used by applications
        typedef char                    int8_t;
        typedef unsigned char           uint8_t;
        typedef short                   int16_t;
        typedef unsigned short          uint16_t;
        typedef int                     int32_t;
        typedef unsigned                uint32_t;
        typedef long long int           int64_t;
        typedef unsigned long long int  uint64_t;

// uintptr_t for C code (pointer-sized unsigned integer)
#if !defined(__uintptr_t_defined)
#  if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__ppc64__) || defined(__mips64)
typedef uint64_t uintptr_t;
#  else
typedef uint32_t uintptr_t;
#  endif
#define __uintptr_t_defined
#endif

        #ifdef __cplusplus
}
#endif

#endif