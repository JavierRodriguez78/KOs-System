#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>
#include <stddef.h>

#define ATTR_DEFAULT 0x07  /* light gray on black */
#define ATTR_HDR     0x0F  /* bright white on black */
#define ATTR_RUNNING 0x0A  /* bright green */
#define ATTR_READY   0x0B  /* bright cyan */
#define ATTR_SLEEP   0x0E  /* bright yellow */
#define ATTR_BLOCK   0x0C  /* bright red */
#define ATTR_IDLE    0x08  /* dark gray */

struct ProcInfo {
    uint32_t pid;
    char state[10];
    uint32_t prio;
    uint32_t time;
    char name[32];
};

int parse_line(const char* line, struct ProcInfo* info) {
    // Expected: "<pid> <state> <prio> <time> <name>"; return 5 only when all parsed
    if (!line || !info) return 0;
    const char* p = line;
    // Skip leading spaces and reject if first non-space isn't a digit
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    if (*p < '0' || *p > '9') return 0;

    // Parse pid
    info->pid = 0;
    int got_pid = 0;
    while (*p >= '0' && *p <= '9') { info->pid = info->pid * 10 + (*p - '0'); ++p; got_pid = 1; }
    if (!got_pid) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse state (non-space token)
    int i = 0; int got_state = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 9) { info->state[i++] = *p++; got_state = 1; }
    info->state[i] = 0;
    if (!got_state) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse prio
    info->prio = 0; int got_prio = 0;
    while (*p >= '0' && *p <= '9') { info->prio = info->prio * 10 + (*p - '0'); ++p; got_prio = 1; }
    if (!got_prio) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse time
    info->time = 0; int got_time = 0;
    while (*p >= '0' && *p <= '9') { info->time = info->time * 10 + (*p - '0'); ++p; got_time = 1; }
    if (!got_time) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse name (token until whitespace)
    i = 0; int got_name = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 31) { info->name[i++] = *p++; got_name = 1; }
    info->name[i] = 0;
    if (!got_name) return 0;

    return 5;
}

static void print_padded(const char* s, int width) {
    if (!s) s = "";
    // print string
    kos_puts((const int8_t*)s);
    // pad to width (left aligned)
    int len = 0; while (s[len] && len < width) len++;
    for (; len < width; ++len) kos_putc(' ');
}

int top_main(int argc, char** argv) {
    (void)argc; (void)argv;
    char buffer[4096];
    while (1) {
        kos_clear();
        kos_set_attr(ATTR_HDR);
        kos_puts("KOS top v3 - Process List\n");
        kos_puts("PID   STATE     PRIO  TIME    NAME\n");
        kos_set_attr(ATTR_DEFAULT);

        // Fetch rows from kernel
        int32_t n = 0;
        if (kos_sys_table()->get_process_info) {
            n = kos_sys_table()->get_process_info(buffer, (int32_t)sizeof(buffer)-1);
            if (n < 0) n = 0;
            if (n >= (int32_t)sizeof(buffer)) n = (int32_t)sizeof(buffer) - 1;
            buffer[n] = 0;
        } else {
            kos_puts("(process info service unavailable)\n");
            n = 0; buffer[0] = 0;
        }

        // Iterate lines
        char* p = buffer;
        while (*p) {
            // find end of line
            char* line = p;
            while (*p && *p != '\n' && *p != '\r') ++p;
            char saved = *p; *p = 0;

            // parse and print
            struct ProcInfo info;
            if (parse_line(line, &info) == 5) {
                // Choose base color by state
                uint8_t base = ATTR_DEFAULT;
                if (strcmp(info.state, "RUNNING") == 0) base = ATTR_RUNNING;
                else if (strcmp(info.state, "READY") == 0) base = ATTR_READY;
                else if (strcmp(info.state, "SLEEPING") == 0) base = ATTR_SLEEP;
                else if (strcmp(info.state, "BLOCKED") == 0) base = ATTR_BLOCK;
                else if (strcmp(info.state, "IDLE") == 0) base = ATTR_IDLE;

                kos_set_attr(base);

                // PID 5, STATE 8 (left)
                kos_printf("%5u  ", (size_t)info.pid);
                print_padded(info.state, 8);

                // Colorize priority: low number = higher priority
                uint8_t prio_attr = (info.prio <= 1) ? 0x0A /*green*/ : (info.prio <= 3 ? 0x0E /*yellow*/ : 0x0C /*red*/);
                kos_set_attr(prio_attr);
                kos_printf("  %2u", (size_t)info.prio);

                // Restore base for remaining columns
                kos_set_attr(base);
                kos_printf("   %6u  ", (size_t)info.time);
                kos_puts((const int8_t*)info.name);
                kos_putc('\n');

                // Reset to default after each row
                kos_set_attr(ATTR_DEFAULT);
            }

            // restore and advance
            *p = saved;
            if (*p == '\n' || *p == '\r') ++p;
            if (*p == '\n' && saved == '\r') ++p; // handle CRLF
        }

        kos_puts("\nPress Ctrl+C to exit\n");
        // Crude delay ~ refresh ~10Hz
        for (volatile int i = 0; i < 5000000; ++i) { }
    }
    return 0;
}
#ifndef APP_EMBED
int main(void) {
    top_main(0, NULL);
    return 0;
}
#endif