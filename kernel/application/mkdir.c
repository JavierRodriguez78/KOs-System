#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"
#include "app_log.h"

static void print_help(void) {
    kos_puts((const int8_t*)"Usage: mkdir [-p] [-h] <dir> [dir2 ...]\n");
    kos_puts((const int8_t*)"  -p   Make parent directories as needed (like UNIX)\n");
    kos_puts((const int8_t*)"  -h   Show this help\n");
}

void app_mkdir(void) {
    int32_t argc = kos_argc();
    int make_parents = 0;

    // Parse options
    int32_t i = 1;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            print_help();
            return;
        }
        if (strcmp(a, (const int8_t*)"-p") == 0) { make_parents = 1; continue; }
        if (strcmp(a, (const int8_t*)"--") == 0) { ++i; break; }
        // first non-option
        break;
    }

    if (i >= argc) {
        kos_puts((const int8_t*)"mkdir: missing operand\n");
        print_help();
        return;
    }

    // Process each directory argument
    for (; i < argc; ++i) {
        const int8_t* path = kos_argv(i);
        if (!path) continue;
        int32_t rc = kos_mkdir(path, make_parents);
        if (rc < 0) {
            app_log((const int8_t*)"mkdir: failed to create '%s'\n", path);
        } else {
            app_log((const int8_t*)"created: %s\n", path);
        }
    }
}

#ifndef APP_EMBED
int main(void) { app_mkdir(); return 0; }
#endif
