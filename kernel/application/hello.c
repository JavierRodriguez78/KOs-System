#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"

void app_hello(void) {
    kos_printf((int8_t*)"Hello from %s %d 0x%X!\n", (int8_t*)"application hello", 42, 0xBEEF);
}

#ifndef APP_EMBED
int main(void) {
    app_hello();
    return 0;
}
#endif
