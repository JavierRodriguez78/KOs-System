#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include "app.hpp"

static void print_kv(const int8_t* key, const int8_t* val) {
    kos_puts(key);
    kos_puts((const int8_t*)": ");
    kos_puts(val ? val : (const int8_t*)"(none)");
}

extern "C" void app_ifconfig(void) {
    // Standalone app prints placeholders; embedded kernel version shows real config.
    kos_puts((const int8_t*)"eth0    Link encap:Ethernet  HWaddr (n/a)\n");
    kos_puts((const int8_t*)"          inet addr: (unknown)  Mask: (unknown)  Gateway: (unknown)\n");
    kos_puts((const int8_t*)"          DNS: (unknown)\n");
    kos_puts((const int8_t*)"          DOWN MTU 1500\n");
    kos_puts((const int8_t*)"          RX packets 0  bytes 0\n");
    kos_puts((const int8_t*)"          TX packets 0  bytes 0\n");
}

#ifndef APP_EMBED
extern "C" int main(void) { app_ifconfig(); return 0; }
#endif
