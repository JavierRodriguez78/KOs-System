#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

static void print_help(void) {
    kos_puts((const int8_t*)"Usage: cd [DIR]\n");
    kos_puts((const int8_t*)"Change the current working directory.\n\n");
    kos_puts((const int8_t*)"Without DIR, cd changes to /home.\n");
    kos_puts((const int8_t*)"DIR may be absolute (e.g., /bin) or relative (e.g., .., bin).\n");
    kos_puts((const int8_t*)"Paths are normalized: '.' and '..' are handled, and extra '/' are collapsed.\n\n");
    kos_puts((const int8_t*)"Options:\n");
    kos_puts((const int8_t*)"  -h, --help   Show this help and exit\n\n");
    kos_puts((const int8_t*)"Examples:\n");
    kos_puts((const int8_t*)"  cd            # go to /home\n");
    kos_puts((const int8_t*)"  cd /          # go to root\n");
    kos_puts((const int8_t*)"  cd ..         # go to parent directory\n");
    kos_puts((const int8_t*)"  cd bin        # enter 'bin' under current directory\n");
    kos_puts((const int8_t*)"  cd ../bin     # relative path with parent traversal\n");
}

void app_cd(void) {
    int32_t argc = kos_argc();
    const int8_t* target = 0;
    if (argc <= 1) {
        // Default directory when no argument
        target = (const int8_t*)"/home";
    } else if (argc == 2) {
        const int8_t* a1 = kos_argv(1);
        if (!a1) {
            print_help();
            return;
        }
        if (strcmp(a1, (const int8_t*)"-h") == 0 || strcmp(a1, (const int8_t*)"--help") == 0) {
            print_help();
            return;
        }
        target = a1;
    } else {
        print_help();
        return;
    }

    int32_t rc = kos_chdir(target);
    if (rc < 0) {
        kos_printf((const int8_t*)"cd: cannot change directory to '%s'\n", target);
    }
}

#ifndef APP_EMBED
int main(void) { app_cd(); return 0; }
#endif
