#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: ls [path]\n");
}

void app_ls(void) {
    // Default to current working directory if available, else root
    const int8_t* path = 0;
    int32_t argc = kos_argc();
    if (argc > 2) { print_usage(); return; }
    if (argc == 2) {
        path = kos_argv(1);
    } else {
        path = kos_cwd();
        if (!path || path[0] == 0) path = (const int8_t*)"/";
    }
    if (path[0] == '/' && path[1] == 0) {
        kos_puts((const int8_t*)"Listing /:\n");
        kos_listroot();
        return;
    }
    kos_puts((const int8_t*)"Listing: ");
    kos_puts(path);
    kos_puts((const int8_t*)"\n");
    kos_listdir(path);
}

#ifndef APP_EMBED
int main(void) {
    app_ls();
    return 0;
}
#endif
