#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.h"
#include <lib/libc/string.h>

// Print the current working directory
// Usage: pwd
void app_pwd(void) {
    // Minimal help handling
    int32_t argc = kos_argc();
    if (argc > 1) {
        const int8_t* a1 = kos_argv(1);
        if (a1 && (strcmp(a1, (const int8_t*)"-h") == 0 || strcmp(a1, (const int8_t*)"--help") == 0)) {
            kos_puts((const int8_t*)"Usage: pwd\n");
            return;
        }
        // Ignore other options/args for now to keep semantics simple
    }

    const int8_t* cwd = kos_cwd();
    if (!cwd || !cwd[0]) cwd = (const int8_t*)"/";
    kos_puts(cwd);
    kos_putc('\n');
}

#ifndef APP_EMBED
int main(void) {
    app_pwd();
    return 0;
}
#endif
