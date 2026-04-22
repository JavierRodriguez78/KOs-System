// Dedicated Hardware Info application wrapper.
// Reuses the current lshw implementation to keep behavior identical.

#define APP_EMBED 1
#include "lshw.c"

int main(void) {
    app_lshw();
    return 0;
}
