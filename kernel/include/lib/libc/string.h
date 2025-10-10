#ifndef  LIBC__STRING_H
#define  LIBC__STRING_H

#ifdef __cplusplus
extern "C" {
#endif

// Minimal C-compatible typedefs for freestanding environment
#if !defined(__stddef_size_t) && !defined(_SIZE_T) && !defined(_SIZE_T_DEFINED) && !defined(__size_t_defined)
typedef unsigned long size_t; // Works for i386; adjust if needed
#define __size_t_defined
#endif

// C standard-like prototypes used by applications
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t len);
size_t strlen(const char* s);

#ifdef __cplusplus
}
#endif

#endif