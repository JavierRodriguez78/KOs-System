#include <stdarg.h>
#include <console/tty.hpp>
#include <lib/stdio.hpp>
#include <common/panic.hpp>

using namespace kos::console;

namespace kos {
namespace kernel {

static bool g_reboot_on_panic = false;

// Port I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret; __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}

// 16550A UART (COM1) minimal output for panic fallback
static inline void uart_out(uint16_t port, uint8_t val) { outb(port, val); }
static inline uint8_t uart_in(uint16_t port) { return inb(port); }
static inline void serial_init_com1() {
    const uint16_t COM1 = 0x3F8;
    // Disable interrupts
    uart_out(COM1 + 1, 0x00);
    // Enable DLAB
    uart_out(COM1 + 3, 0x80);
    // Set baud rate divisor to 1 (115200 baud)
    uart_out(COM1 + 0, 0x01);
    uart_out(COM1 + 1, 0x00);
    // 8 bits, no parity, one stop bit
    uart_out(COM1 + 3, 0x03);
    // Enable FIFO, clear them, 14-byte threshold
    uart_out(COM1 + 2, 0xC7);
    // IRQs disabled, RTS/DSR set
    uart_out(COM1 + 4, 0x0B);
}

static inline void serial_putc_com1(char c) {
    const uint16_t COM1 = 0x3F8;
    // Wait for Transmitter Holding Register Empty (LSR bit 5)
    while ((uart_in(COM1 + 5) & 0x20) == 0) { }
    uart_out(COM1 + 0, (uint8_t)c);
}

static inline void serial_write_com1(const char* s) {
    if (!s) return;
    // Try to detect if port responds; if LSR reads as 0xFF, assume no UART
    const uint16_t COM1 = 0x3F8;
    // best-effort init (idempotent for our simple setup)
    serial_init_com1();
    while (*s) {
        if (*s == '\n') serial_putc_com1('\r');
        serial_putc_com1(*s++);
    }
}

// Minimal serial vprintf mirroring kos::sys::vprintf formatting for %c %s %d %u %x %X %p
static void serial_vprintf(const char* fmt, va_list ap) {
    auto print_uint = [](uint32_t v, uint32_t base, bool upper) {
        char buf[32];
        const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        int i = 0;
        if (v == 0) { buf[i++] = '0'; }
        else { while (v && i < (int)sizeof(buf)) { buf[i++] = digs[v % base]; v /= base; } }
        while (i--) serial_putc_com1(buf[i]);
    };
    for (int i = 0; fmt && fmt[i]; ++i) {
        if (fmt[i] != '%') { serial_putc_com1(fmt[i]); continue; }
        ++i; char spec = fmt[i];
        switch (spec) {
            case '%': serial_putc_com1('%'); break;
            case 'c': serial_putc_com1((char)va_arg(ap, int)); break;
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                while (*s) { if (*s=='\n') serial_putc_com1('\r'); serial_putc_com1(*s++); }
                break;
            }
            case 'd': case 'i': {
                int v = va_arg(ap, int);
                if (v < 0) { serial_putc_com1('-'); print_uint((uint32_t)(-v), 10, false); }
                else print_uint((uint32_t)v, 10, false);
                break;
            }
            case 'u': {
                uint32_t v = va_arg(ap, uint32_t);
                print_uint(v, 10, false);
                break;
            }
            case 'x': {
                uint32_t v = va_arg(ap, uint32_t);
                print_uint(v, 16, false);
                break;
            }
            case 'X': {
                uint32_t v = va_arg(ap, uint32_t);
                print_uint(v, 16, true);
                break;
            }
            case 'p': {
                uint32_t v = (uint32_t)va_arg(ap, void*);
                serial_write_com1("0x");
                print_uint(v, 16, false);
                break;
            }
            default:
                serial_putc_com1('%'); serial_putc_com1(spec);
                break;
        }
    }
}

static inline void cli() {
    __asm__ __volatile__("cli");
}

[[noreturn]] static void halt_forever() {
    for(;;) {
        __asm__ __volatile__("hlt");
    }
}

[[noreturn]] void Panic(const char* fmt, ...) {
    // Best-effort: make the panic visible
    TTY::SetColor(15 /*white*/, 4 /*red*/);
    TTY::Write((const int8_t*)"\n================ KERNEL PANIC ================\n");

    // Print formatted message (to both screen and serial)
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    serial_write_com1("[panic] ");
    serial_vprintf(fmt, ap2);
    va_end(ap2);
    kos::sys::vprintf((const int8_t*)fmt, ap);
    va_end(ap);
    TTY::Write((const int8_t*)"\nSystem halted.\n");
    serial_write_com1("\nSystem halted.\n");

    // Stop the world
    cli();
    if (g_reboot_on_panic) {
        // Try keyboard controller reset
        // Wait until input buffer empty (status port 0x64, bit 1 = input buffer full)
        while (inb(0x64) & 0x02) { }
        outb(0x64, 0xFE);
        // If that failed, fallback to HLT loop
    }
    halt_forever();
}

extern "C" [[noreturn]] void kernel_panic(const char* msg) {
    Panic("%s", msg ? msg : "(null)");
}

} // namespace kernel
} // namespace kos

namespace kos { namespace kernel {
void SetPanicReboot(bool enabled) { g_reboot_on_panic = enabled; }
} }
