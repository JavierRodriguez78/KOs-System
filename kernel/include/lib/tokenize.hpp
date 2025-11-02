#ifndef KOS_LIB_TOKENIZE_HPP
#define KOS_LIB_TOKENIZE_HPP

#include <lib/libc/stdint.h>

namespace kos {
namespace lib {

// Split a comma-separated list in-place; returns number of items and fills arr with pointers into the buffer.
// buf: mutable buffer containing a CSV string; will be modified (commas -> NUL, trims simple spaces)
// arr: output array of pointers into buf
// maxItems: capacity of arr
int split_csv(int8_t* buf, const char** arr, int maxItems);

// Split a line into argv tokens separated by spaces and tabs (no quotes/escapes). Returns argc.
// line: mutable buffer; will be modified (spaces -> NUL)
// argv: output array of pointers into line
// maxArgs: capacity of argv
int split_args(int8_t* line, const int8_t** argv, int maxArgs);

} // namespace lib
} // namespace kos

#endif // KOS_LIB_TOKENIZE_HPP
