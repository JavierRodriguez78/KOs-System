#ifndef KOS_LIB_LIBC_TOKENIZE_H
#define KOS_LIB_LIBC_TOKENIZE_H

#include <lib/libc/stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Split a buffer into argv tokens separated by spaces/tabs (no quotes/escapes).
// Modifies buf in-place by inserting NULs. Returns argc.
int kos_split_args(int8_t* buf, const int8_t** argv, int maxArgs);

// Build an absolute program path into out buffer.
// If prog is absolute (starts with '/'), copies as-is.
// Otherwise builds "/bin/<prog>" and appends ".elf" if not already present.
// Returns length written (excluding NUL), or -1 on error.
int kos_build_prog_path(const int8_t* prog, int8_t* out, int outSize);

#ifdef __cplusplus
}
#endif

#endif // KOS_LIB_LIBC_TOKENIZE_H
