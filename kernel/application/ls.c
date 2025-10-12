#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: ls [-l] [-a] [path]\n");
}

void app_ls(void) {
    // Default to current working directory if available, else root
    const int8_t* path = 0;
    uint32_t flags = 0; // KOS_LS_FLAG_LONG | KOS_LS_FLAG_ALL
    int32_t argc = kos_argc();
    // Parse options
    int i = 1;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) { print_usage(); return; }
        if (strcmp(a, (const int8_t*)"-l") == 0) { flags |= KOS_LS_FLAG_LONG; continue; }
        if (strcmp(a, (const int8_t*)"-a") == 0) { flags |= KOS_LS_FLAG_ALL; continue; }
        if (strcmp(a, (const int8_t*)"--") == 0) { ++i; break; }
        break; // first non-option
    }
    if (i < argc) path = kos_argv(i);
    if (!path) { path = kos_cwd(); if (!path || path[0] == 0) path = (const int8_t*)"/"; }
    if (path[0] == '/' && path[1] == 0) {
        kos_puts((const int8_t*)"Listing /:\n");
        kos_listroot();
        return;
    }
    kos_puts((const int8_t*)"Listing: ");
    kos_puts(path);
    kos_puts((const int8_t*)"\n");
    kos_listdir_ex(path, flags);
}

#ifndef APP_EMBED
int main(void) {
    app_ls();
    return 0;
}
#endif
