#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: cat [-h] <file> [file...]\n");
    kos_puts((const int8_t*)"Show the contents of one or more files.\n");
}

void app_cat(void) {
    const int32_t argc = kos_argc();
    if (argc <= 1) { print_usage(); return; }

    // Parse options
    int i = 1;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            print_usage();
            return;
        }
        if (strcmp(a, (const int8_t*)"--") == 0) { ++i; break; }
        // first non-option
        if (a[0] != '-') break;
        // Unknown option
        kos_puts((const int8_t*)"cat: unknown option: ");
        kos_puts(a);
        kos_puts((const int8_t*)"\n");
        print_usage();
        return;
    }

    if (i >= argc) { print_usage(); return; }

    // Static buffer for reading files
    static uint8_t buf[64 * 1024]; // 64 KiB

    for (; i < argc; ++i) {
        const int8_t* path = kos_argv(i);
        if (!path || !path[0]) continue;
        int32_t n = kos_readfile(path, buf, sizeof(buf));
        if (n < 0) {
            kos_puts((const int8_t*)"cat: cannot read file: ");
            kos_puts(path);
            kos_puts((const int8_t*)"\n");
            continue;
        }
        // Print exactly n bytes
        for (int32_t k = 0; k < n; ++k) {
            kos_putc((int8_t)buf[k]);
        }
        // If multiple files, add a trailing newline between files (only if file didn't end with one)
        if (i + 1 < argc) {
            if (n == 0 || buf[n-1] != '\n') kos_putc('\n');
        }
    }
}

#ifndef APP_EMBED
int main(void) {
    app_cat();
    return 0;
}
#endif
