#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"
#include "app_log.h"

static void print_help(void) {
    kos_puts((const int8_t*)"Usage: mv [-h] <src> <dst>\n");
    kos_puts((const int8_t*)"Moves/renames a file or directory.\n");
}

// Use kos_rename wrapper; if kernel doesn't provide it, wrapper returns -1.

void app_mv(void) {
    int32_t argc = kos_argc();
    // Parse options
    int32_t i = 1;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            print_help();
            return;
        }
        if (strcmp(a, (const int8_t*)"--") == 0) { ++i; break; }
        // first non-option
        break;
    }

    if (i + 1 >= argc) {
        kos_puts((const int8_t*)"mv: missing operand\n");
        print_help();
        return;
    }

    const int8_t* src = kos_argv(i);
    const int8_t* dst = kos_argv(i + 1);

    if (!src || !dst || !src[0] || !dst[0]) {
        kos_puts((const int8_t*)"mv: invalid path(s)\n");
        return;
    }

    int32_t rc = kos_rename(src, dst);
    if (rc < 0) {
        // Explain limitation and suggest workaround.
        app_log((const int8_t*)"mv: operation not supported by kernel API yet.\n");
        app_log((const int8_t*)"Hint: use cp + rm once those commands exist, or add a rename syscall.\n");
    } else {
        app_log((const int8_t*)"moved: %s -> %s\n", src, dst);
    }
}

#ifndef APP_EMBED
int main(void) { app_mv(); return 0; }
#endif
