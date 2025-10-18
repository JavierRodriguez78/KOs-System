#include <lib/libc/stdio.h>

void app_clear(void) {
    // Clear text screen via system API
    kos_clear();
}

#ifndef APP_EMBED
int main(void) {
    app_clear();
    return 0;
}
#endif
