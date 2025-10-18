// Deprecated compatibility header
// Historically defined a partial ApiTableC and a separate API surface.
// To avoid ABI/layout mismatches with applications, this header now
// forwards to the authoritative libc stdio API, which defines the full
// ApiTableC with the correct field order and inline wrappers.
#ifndef KOS_LIBC_API_H
#define KOS_LIBC_API_H

#include <lib/libc/stdio.h>

#endif // KOS_LIBC_API_H
