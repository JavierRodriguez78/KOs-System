// Real hardware reboot for KOS
#include <stdint.h>
#include "app.h"

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void reboot_hw() {
    // Try keyboard controller method (0x64, 0xFE)
    outb(0x64, 0xFE);
    // If that fails, triple fault (load null IDT)
    __asm__ volatile (
        "lidt (%0)\n\t"
        "int $0x03\n\t"
        : : "r"((uint8_t[6]){0})
    );
    // Hang if still running
    for (;;) __asm__ volatile ("hlt");
}

void app_reboot(void) {
    reboot_hw();
}

int main(void) {
    app_reboot();
    return 0;
}
