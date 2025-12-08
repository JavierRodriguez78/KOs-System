#include "include/net/raw_icmp.hpp"
#include "include/net/icmp.hpp"
#include "include/common/types.hpp"

// Provide C ABI functions for ping.c to call, backed by kos::net::rawicmp_*.
extern "C" {

struct _shim_fd_entry {
    int used;
    kos::net::RawIcmpHandle h;
    kos::common::uint32_t dst_ip_be;
};

static _shim_fd_entry _fdtab[8]; // tiny fixed table

static int _fd_alloc() {
    for (int i = 0; i < 8; ++i) {
        if (!_fdtab[i].used) { _fdtab[i].used = 1; return i + 1; }
    }
    return -1;
}

static _shim_fd_entry* _fd_get(int fd) {
    if (fd <= 0 || fd > 8) return 0;
    _shim_fd_entry* e = &_fdtab[fd - 1];
    return e->used ? e : 0;
}

static kos::common::uint32_t parse_ipv4_be(const char* s) {
    if (!s) return 0;
    unsigned parts[4] = {0,0,0,0};
    int pi = 0;
    unsigned val = 0;
    for (int i = 0; s[i] != '\0'; ++i) {
        char ch = s[i];
        if (ch >= '0' && ch <= '9') {
            val = val * 10 + (unsigned)(ch - '0');
            if (val > 255) return 0;
        } else if (ch == '.') {
            if (pi >= 4) return 0;
            parts[pi++] = val;
            val = 0;
        } else {
            return 0;
        }
    }
    if (pi != 3) return 0;
    parts[3] = val;
    kos::common::uint32_t be = (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
    return be;
}

int kos_sock_open_raw_icmp(const char* dst_ip) {
    kos::common::uint32_t be = parse_ipv4_be(dst_ip);
    if (be == 0) return -1;
    int fd = _fd_alloc();
    if (fd < 0) return -1;
    _shim_fd_entry* e = _fd_get(fd);
    e->h = kos::net::rawicmp_open();
    e->dst_ip_be = be;
    return fd;
}

int kos_sock_send_icmp_echo(int fd, unsigned short ident, unsigned short seq, const void* payload, unsigned short len) {
    _shim_fd_entry* e = _fd_get(fd);
    if (!e) return -1;
    bool ok = kos::net::rawicmp_send_echo(e->h, e->dst_ip_be, ident, seq,
                                          (const kos::common::uint8_t*)payload,
                                          (kos::common::uint32_t)len,
                                          1000);
    return ok ? (int)len : -1;
}

int kos_sock_recv_icmp_echo(int fd, unsigned short ident, unsigned short seq, void* buf, unsigned short buflen, unsigned int timeout_ms) {
    _shim_fd_entry* e = _fd_get(fd);
    if (!e) return -1;
    kos::common::uint32_t n = kos::net::rawicmp_recv_echo(e->h, ident, seq, (kos::common::uint8_t*)buf, buflen, timeout_ms);
    return (int)n;
}

void kos_sock_close(int fd) {
    _shim_fd_entry* e = _fd_get(fd);
    if (!e) return;
    kos::net::rawicmp_close(e->h);
    e->used = 0;
}

unsigned long long kos_monotonic_ms(void) {
    // Stub: no monotonic clock wired
    return 0ULL;
}

}
