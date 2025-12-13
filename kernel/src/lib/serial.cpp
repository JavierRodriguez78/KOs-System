#include <lib/serial.hpp>
#include <common/types.hpp>

namespace kos {
namespace lib {

using kos::common::uint8_t;
using kos::common::uint16_t;

// Port I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const uint16_t COM1 = 0x3F8;
static bool initialized = false;

void serial_init() {
    if (initialized) return;
    // Disable interrupts
    outb(COM1 + 1, 0x00);
    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 3, 0x80);
    // Set divisor to 1 (115200 baud)
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    // 8 bits, no parity, one stop bit
    outb(COM1 + 3, 0x03);
    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 2, 0xC7);
    // IRQs disabled, RTS/DSR set
    outb(COM1 + 4, 0x0B);
    initialized = true;
}

void serial_putc(char c) {
    // Wait for Transmitter Holding Register Empty (LSR bit 5)
    for (int timeout = 0; timeout < 100000; ++timeout) {
        if (inb(COM1 + 5) & 0x20) break;
    }
    outb(COM1 + 0, (uint8_t)c);
}

void serial_write(const char* s) {
    if (!s) return;
    if (!initialized) serial_init();
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

} // namespace lib
} // namespace kos
