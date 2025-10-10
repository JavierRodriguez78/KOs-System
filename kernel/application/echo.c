#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"

void app_echo(void) {
    kos_puts((int8_t*)"Echo app says hi.\n");
}

#ifndef APP_EMBED
int main(void) {
    app_echo();
    return 0;
}
#endif
