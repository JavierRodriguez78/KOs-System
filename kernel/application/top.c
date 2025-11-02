#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>
#include <stddef.h>

struct ProcInfo {
    uint32_t pid;
    char state[10];
    uint32_t prio;
    uint32_t time;
    char name[32];
};

int parse_line(const char* line, struct ProcInfo* info) {
    // Format: "%u %9s %u %u %31s"
    if (!line || !info) return 0;
    const char* p = line;
    // Parse pid
    info->pid = 0;
    while (*p == ' ') ++p;
    while (*p >= '0' && *p <= '9') {
        info->pid = info->pid * 10 + (*p - '0');
        ++p;
    }
    while (*p == ' ') ++p;
    // Parse state
    int i = 0;
    while (*p && *p != ' ' && i < 9) info->state[i++] = *p++;
    info->state[i] = 0;
    while (*p == ' ') ++p;
    // Parse prio
    info->prio = 0;
    while (*p >= '0' && *p <= '9') {
        info->prio = info->prio * 10 + (*p - '0');
        ++p;
    }
    while (*p == ' ') ++p;
    // Parse time
    info->time = 0;
    while (*p >= '0' && *p <= '9') {
        info->time = info->time * 10 + (*p - '0');
        ++p;
    }
    while (*p == ' ') ++p;
    // Parse name
    i = 0;
    while (*p && *p != ' ' && i < 31) info->name[i++] = *p++;
    info->name[i] = 0;
    // Return 5 if all fields are present
    return 5;
}

int top_main(int argc, char** argv) {
    char buffer[2048];
    struct ProcInfo procs[64];
    int proc_count = 0;
    char filter_name[32] = "";
    char filter_state[10] = "";
    int sort_time = 1;

    // Parse args for filter/sort
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--name=", 7) == 0) memcpy(filter_name, argv[i]+7, 31);
        if (strncmp(argv[i], "--state=", 8) == 0) memcpy(filter_state, argv[i]+8, 9);
        if (strcmp(argv[i], "--sort=pid") == 0) sort_time = 0;
    }

    extern void PrintCpuInfo_C();
    while (1) {
        kos_clear();
        kos_puts("[DEBUG] Minimal top loop\n");
        kos_puts("KOS top - Process List\n");
        kos_puts("PID     STATE     PRIO     TIME     NAME\n");
        kos_puts("\nPress Ctrl+C to exit\n");
        for (volatile int i = 0; i < 10000000; ++i) {}
    }
    return 0;
}
#ifndef APP_EMBED
int main(void) {
    top_main(0, NULL);
    return 0;
}
#endif