#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"


void app_echo(void) {
    int32_t argc = kos_argc();
    if (argc <= 1) {
        // No args: print newline like standard echo
        kos_puts((const int8_t*)"\n");
        return;
    }

    // Options: -h/--help, -n (no trailing newline), -- (end of options)
    int32_t i = 1;
    int no_newline = 0;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) break;
        if (strcmp(a, (const int8_t*)"--") == 0) { ++i; break; }
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            kos_puts((const int8_t*)"Usage: echo [-n] [--] [strings...]\n");
            kos_puts((const int8_t*)"  -n       Do not output the trailing newline\n");
            kos_puts((const int8_t*)"  -h,--help  Show this help\n");
            return;
        }
        if (strcmp(a, (const int8_t*)"-n") == 0) { no_newline = 1; continue; }
        // Non-option: start printing from here
        break;
    }

    // Print remaining args separated by single spaces
    int first = 1;
    for (; i < argc; ++i) {
        const int8_t* s = kos_argv(i);
        if (!s) continue;
        if (!first) kos_putc(' ');
        first = 0;
        kos_puts(s);
    }
    if (!no_newline) kos_putc('\n');
}

#ifndef APP_EMBED
int main(void) { app_echo(); return 0; }
#endif
