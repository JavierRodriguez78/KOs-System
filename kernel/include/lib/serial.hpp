#pragma once

namespace kos {
namespace lib {

// Simple serial output to COM1 for debugging
void serial_init();
void serial_write(const char* s);
void serial_putc(char c);

} // namespace lib
} // namespace kos
