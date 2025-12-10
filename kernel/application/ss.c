// Enable real path using the kernel ABI (returns 0 sockets for now)
#define KOS_NET_HAVE_SOCKETS 1

#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>

// ss -tulnp like placeholder for KOS
// Currently, KOS does not provide a TCP/UDP stack or socket table.
// This utility parses common ss flags and shows a friendly message.
// When networking matures, enable a real listing by defining KOS_NET_HAVE_SOCKETS
// and providing a C ABI to enumerate sockets.

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: ss [-t] [-u] [-l] [-n] [-p]\n");
}

// Use kos_sockinfo_t and kos_net_list_sockets from libc API when enabled

int app_ss(void) {
    int32_t argc = kos_argc();
    // Flags
    int want_tcp = 0, want_udp = 0, listening_only = 0, numeric = 0, with_prog = 0;

    for (int i = 1; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) { print_usage(); return 0; }
        if (strcmp(a, (const int8_t*)"-t") == 0) { want_tcp = 1; continue; }
        if (strcmp(a, (const int8_t*)"-u") == 0) { want_udp = 1; continue; }
        if (strcmp(a, (const int8_t*)"-l") == 0) { listening_only = 1; continue; }
        if (strcmp(a, (const int8_t*)"-n") == 0) { numeric = 1; continue; }
        if (strcmp(a, (const int8_t*)"-p") == 0) { with_prog = 1; continue; }
    }
    (void)numeric; (void)with_prog; // unused in placeholder

#ifdef KOS_NET_HAVE_SOCKETS
    // Real socket listing path (when available)
    kos_sockinfo_t buf[64];
    if (!want_tcp && !want_udp) { want_tcp = 1; want_udp = 1; }
    int n = kos_net_list_sockets(buf, (int)(sizeof(buf)/sizeof(buf[0])), want_tcp, want_udp, listening_only);
    if (n < 0) {
        kos_puts((const int8_t*)"ss: kernel socket enumeration failed\n");
        return -1;
    }
    kos_puts((const int8_t*)"Proto   State      Local Address:Port           Peer Address:Port     PID/Program\n");
    for (int i = 0; i < n; ++i) {
        char line[128];
        // PID/Program field
        char pp[32]; pp[0] = 0;
        if (buf[i].pid > 0 && buf[i].prog && buf[i].prog[0]) {
            snprintf(pp, sizeof(pp), "%d/%s", buf[i].pid, buf[i].prog);
        }
        snprintf(line, sizeof(line), "%s     %-9s %-21s:%u %-21s:%u %s\n",
                 buf[i].proto, buf[i].state ? buf[i].state : "",
                 buf[i].laddr ? buf[i].laddr : "*", (unsigned)buf[i].lport,
                 buf[i].raddr ? buf[i].raddr : "*", (unsigned)buf[i].rport,
                 pp);
        kos_puts((const int8_t*)line);
    }
    if (n == 0) {
        kos_puts((const int8_t*)"No matching sockets.\n");
    }
    return 0;
#else
    // Placeholder: no TCP/UDP stack available
    kos_puts((const int8_t*)"Proto   State      Local Address:Port           Peer Address:Port     PID/Program\n");
    kos_puts((const int8_t*)"(no TCP/UDP sockets: network stack not available)\n");
    kos_puts((const int8_t*)"Hint: enable NIC + TCP/IP stack; then provide socket enumeration API.\n");
    return 0;
#endif
}

#ifndef APP_EMBED
int main(void) {
    return app_ss();
}
#endif
