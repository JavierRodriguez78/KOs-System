// Dedicated Process Monitor application wrapper.
// Reuses the current top implementation to keep behavior identical.

#define APP_EMBED 1
#include "top.c"

int main(void) {
    return top_main(0, 0);
}
