#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"

void app_ls(void) {
    kos_puts((int8_t*)"Listing root directory:\n");
    kos_listroot();
}

#ifndef APP_EMBED
int main(void) {
    app_ls();
    return 0;
}
#endif
