// Linux-like shutdown command for KOS
#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Attempt to power off under common emulators/hardware
static void poweroff_hw() {
    // QEMU (ISA) uses port 0x604 with value 0x2000
    outw(0x604, 0x2000);
    // Bochs/old QEMU uses port 0xB004 with value 0x0000
    outw(0xB004, 0x0000);
    // VMware sometimes uses 0x10 with magic
    outb(0x10, 0x00);
    // If still running, halt CPU
    for(;;) { __asm__ __volatile__("cli; hlt"); }
}

// Basic help text compatible with common Linux flags subset
static void print_help() {
    kos_puts((const int8_t*)"Usage: shutdown [-h|-P|-r] [now] [message]\n");
    kos_puts((const int8_t*)"  -h, -P   Halt and power off the system\n");
    kos_puts((const int8_t*)"  -r       Reboot the system\n");
    kos_puts((const int8_t*)"  now      Execute immediately (default)\n");
}

void app_shutdown(void) {
    int32_t argc = kos_argc();
    int32_t do_reboot = 0;
    int32_t do_poweroff = 1; // default to poweroff

    // Parse minimal flags
    for (int32_t i = 1; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"-P") == 0) {
            do_poweroff = 1; do_reboot = 0;
            continue;
        }
        if (strcmp(a, (const int8_t*)"-r") == 0) {
            do_reboot = 1; do_poweroff = 0;
            continue;
        }
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            print_help();
            return;
        }
        // Accept and ignore "now" and any trailing message (not implemented yet)
    }

    if (do_reboot) {
        kos_puts((const int8_t*)"Rebooting...\n");
        // Inline reboot to avoid linking against separate reboot app
        // Try keyboard controller (0x64, 0xFE), then triple-fault fallback
        __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
        struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
        __asm__ __volatile__("lidt %0\n\tint $0x03" : : "m"(null_idt));
        for(;;) { __asm__ __volatile__("cli; hlt"); }
    }

    kos_puts((const int8_t*)"Powering off...\n");
    poweroff_hw();
}

#ifndef APP_EMBED
int main(void) {
    app_shutdown();
    return 0;
}
#endif
