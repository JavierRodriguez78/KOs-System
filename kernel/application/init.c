#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>
#include <lib/libc/tokenize.h>

static const int8_t* rc_paths[] = {
    (const int8_t*)"/etc/init.d/rc.local",
    (const int8_t*)"/ETC/INIT.D/RC.LOCAL",
};


void app_init(void) {
    kos_chdir((const int8_t*)"/");

    kos_puts((const int8_t*)"init: starting rc.local...\n");
    // Load rc.local
    #define INIT_MAX_RC 8192
    static uint8_t buf[INIT_MAX_RC];
    int32_t n = -1; const int8_t* used = 0;
    for (unsigned i = 0; i < sizeof(rc_paths)/sizeof(rc_paths[0]); ++i) {
        n = kos_readfile(rc_paths[i], buf, INIT_MAX_RC - 1);
        if (n > 0) { used = rc_paths[i]; break; }
    }
    if (n <= 0) {
        kos_puts((const int8_t*)"init: rc.local not found; continuing\n");
        return;
    }
    buf[n] = 0;
    kos_puts((const int8_t*)"init: executing "); kos_puts(used); kos_puts((const int8_t*)"\n");

    int8_t* cur = (int8_t*)buf;
    while (*cur) {
        int8_t* line = cur;
        // find end of line
        int8_t* nl = (int8_t*)memchr(cur, '\n', (buf + n) - (uint8_t*)cur);
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += (int32_t)strlen((const char*)cur); }
        // trim
        int8_t* p = line; while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == 0 || *p == '#') continue;
        // copy to tmp for tokenization
        int8_t tmp[160]; int i = 0; while (p[i] && i < (int)sizeof(tmp)-1) { tmp[i] = p[i]; ++i; } tmp[i] = 0;
        // split
        const int32_t MAX_ARGS = 16; const int8_t* argv[MAX_ARGS];
        int argc = kos_split_args(tmp, argv, MAX_ARGS);
        if (argc <= 0) continue;
        int8_t elfPath[96];
        if (kos_build_prog_path(argv[0], elfPath, (int)sizeof(elfPath)) < 0) {
            kos_puts((const int8_t*)"init: bad path, skipping\n");
            continue;
        }
        kos_puts((const int8_t*)"init: exec "); kos_puts(elfPath); kos_puts((const int8_t*)"\n");
        if (kos_exec(elfPath, argc, argv, p) < 0) {
            kos_puts((const int8_t*)"init: exec failed\n");
        }
    }
}

#ifndef APP_EMBED
int main(void) { app_init(); return 0; }
#endif
