#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>

// Simple ping implementation.
// Usage: ping [-c COUNT] TARGET
// Current behavior (default): simulated ICMP echo replies (no real network stack yet).
// Future behavior: if the network stack exposes raw/datagram ICMP sockets, this file
// will transparently switch to real echo requests when KOS_NET_HAVE_RAW_ICMP is defined.
//
// Hook plan (define when ready):
//   #define KOS_NET_HAVE_RAW_ICMP 1
//   Required APIs (proposed minimal C ABI):
//     int kos_sock_open_raw_icmp(const char* dst_ip); // returns fd or <0
//     int kos_sock_send_icmp_echo(int fd, uint16_t ident, uint16_t seq,
//                                 const void* payload, uint16_t len); // returns bytes sent
//     int kos_sock_recv_icmp_echo(int fd, uint16_t ident, uint16_t seq,
//                                 void* buf, uint16_t buflen, uint32_t timeout_ms); // returns bytes or <0
//     void kos_sock_close(int fd);
//   Optional timing source:
//     uint64_t kos_monotonic_ms(); // milliseconds since boot (or similar)
//
// Rationale: keeping simulation path means the command remains useful before
// networking is implemented; the real path can be enabled by defining the macro
// and providing the above functions in the net stack.

#include <lib/libc/stdint.h>

#ifdef KOS_NET_HAVE_RAW_ICMP
// Forward declarations (expected to be provided by the networking stack once ready).
int kos_sock_open_raw_icmp(const char* dst_ip);
int kos_sock_send_icmp_echo(int fd, uint16_t ident, uint16_t seq, const void* payload, uint16_t len);
int kos_sock_recv_icmp_echo(int fd, uint16_t ident, uint16_t seq, void* buf, uint16_t buflen, uint32_t timeout_ms);
void kos_sock_close(int fd);
uint64_t kos_monotonic_ms(void); // Optional; if absent, RTT falls back to simulated path.
#endif

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: ping [-c COUNT] TARGET\n");
}

static int simulated_ping(const int8_t* target, int count) {
    kos_printf((const int8_t*)"PING %s (simulated): %d fake echoes\n", target, count);
    uint32_t base_heap = kos_get_heap_used();
    for (int seq = 0; seq < count; ++seq) {
        volatile uint32_t spin = 10000 + (uint32_t)seq * 500; // adjustable if too slow
        for (volatile uint32_t i = 0; i < spin; ++i) {}
        uint32_t heap_now = kos_get_heap_used();
        uint32_t delta = (heap_now >= base_heap) ? (heap_now - base_heap) : 0;
        uint32_t rtt = (spin / 1000) + (delta % 7);
        if (rtt < 1) rtt = 1;
        kos_printf((const int8_t*)"64 bytes from %s: icmp_seq=%d ttl=64 time=%u ms\n", target, seq, rtt);
    }
    kos_printf((const int8_t*)"--- %s ping statistics ---\n", target);
    kos_printf((const int8_t*)"%d packets transmitted, %d received, 0%% packet loss, time %u ms\n", count, count, count * 2u);
    return 0;
}

#ifdef KOS_NET_HAVE_RAW_ICMP
// Real ping path using future raw ICMP socket support.
static int real_ping(const int8_t* target, int count) {
    // For now we craft a small payload; in future this could be timestamp + padding.
    const char payload[] = "KOS"; // 3 bytes
    uint16_t ident = 0x1234; // arbitrary identifier (could be process id)
    int fd = kos_sock_open_raw_icmp((const char*)target);
    if (fd < 0) {
        kos_printf((const int8_t*)"ping: unable to open raw ICMP socket for %s (fd=%d)\n", target, fd);
        return -1;
    }
    int received = 0;
    uint64_t start_ms = kos_monotonic_ms ? kos_monotonic_ms() : 0;
    for (int seq = 0; seq < count; ++seq) {
        uint64_t t0 = kos_monotonic_ms ? kos_monotonic_ms() : 0;
        if (kos_sock_send_icmp_echo(fd, ident, (uint16_t)seq, payload, sizeof(payload)) < 0) {
            kos_printf((const int8_t*)"ping: send failed seq=%d\n", seq);
            continue;
        }
        char buf[128];
        int bytes = kos_sock_recv_icmp_echo(fd, ident, (uint16_t)seq, buf, sizeof(buf), 1000); // 1s timeout
        uint64_t t1 = kos_monotonic_ms ? kos_monotonic_ms() : 0;
        if (bytes >= 0) {
            ++received;
            uint32_t rtt = (uint32_t)(kos_monotonic_ms ? (t1 - t0) : 1);
            kos_printf((const int8_t*)"%d bytes from %s: icmp_seq=%d ttl=64 time=%u ms\n", bytes, target, seq, rtt);
        } else {
            kos_printf((const int8_t*)"Request timeout for icmp_seq %d\n", seq);
        }
    }
    uint64_t end_ms = kos_monotonic_ms ? kos_monotonic_ms() : start_ms + (uint64_t)count;
    uint64_t total = end_ms - start_ms;
    kos_printf((const int8_t*)"--- %s ping statistics ---\n", target);
    kos_printf((const int8_t*)"%d packets transmitted, %d received, %d%% packet loss, time %u ms\n",
               count, received, (count - received) * 100 / count, (uint32_t)total);
    kos_sock_close(fd);
    return 0;
}
#endif

int app_ping(void) {
    int32_t argc = kos_argc();
    if (argc <= 1) { print_usage(); return -1; }
    int count = 4; // default like traditional ping
    const int8_t* target = 0;

    for (int i = 1; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) { print_usage(); return 0; }
        if (strncmp((const char*)a, "-c", 2) == 0) {
            // -cCOUNT or -c COUNT
            const char* val = (const char*)a + 2;
            if (*val == 0 && i + 1 < argc) { val = (const char*)kos_argv(++i); }
            int n = 0; while (*val >= '0' && *val <= '9') { n = n * 10 + (*val - '0'); ++val; }
            if (n > 0) count = n; continue;
        }
        // First non-option is target
        if (!target) target = a;
    }

    if (!target) { print_usage(); return -1; }

    // Choose path: real raw ICMP vs simulated.
#ifdef KOS_NET_HAVE_RAW_ICMP
    return real_ping(target, count);
#else
    return simulated_ping(target, count);
#endif
}

#ifndef APP_EMBED
int main(void) {
    return app_ping();
}
#endif
