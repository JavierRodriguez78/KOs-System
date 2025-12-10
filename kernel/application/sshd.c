#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Provide C prototypes matching lib/socket.hpp helpers for C app build
int SocketListenInet(int type /*1=DGRAM,2=STREAM*/, unsigned port);
#ifdef __cplusplus
}
#endif

// Minimal sshd stub: registers a TCP listen socket on port 22.
// There is no SSH protocol nor TCP stack yet; this is for visibility via `ss`.

static void print_usage(void) {
    kos_puts((const int8_t*)"Usage: sshd [port]\n");
}

int app_sshd(void) {
    int32_t argc = kos_argc();
    unsigned port = 22;
    if (argc >= 2) {
        const int8_t* a = kos_argv(1);
        unsigned v = 0;
        for (int i = 0; a && a[i]; ++i) {
            if (a[i] >= '0' && a[i] <= '9') v = v * 10 + (unsigned)(a[i]-'0'); else break;
        }
        if (v > 0 && v < 65536u) port = v;
    }
    int fd = SocketListenInet(2 /*STREAM*/, port);
    if (fd < 0) {
        kos_puts((const int8_t*)"sshd: failed to listen\n");
        return -1;
    }
    kos_printf((const int8_t*)"sshd: listening on tcp 0.0.0.0:%u (fd=%d)\n", port, fd);
    return 0;
}

#ifndef APP_EMBED
int main(void) { return app_sshd(); }
#endif
