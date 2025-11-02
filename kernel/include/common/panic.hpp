#pragma once
#ifndef KOS_COMMON_PANIC_HPP
#define KOS_COMMON_PANIC_HPP

#include <stdarg.h>

namespace kos {
namespace kernel {

// Immediately stops the system: prints a panic banner and message, disables interrupts, halts CPU.
// This function does not return.
[[noreturn]] void Panic(const char* fmt, ...);

// C-friendly variant with a simple string
extern "C" [[noreturn]] void kernel_panic(const char* msg);

// Configure whether Panic should attempt a reboot instead of halting
void SetPanicReboot(bool enabled);

} // namespace kernel
} // namespace kos

// Lightweight assertion macro
#ifndef KASSERT
#define KASSERT(cond) do { \
    if (!(cond)) { kos::kernel::Panic("Assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__); } \
} while(0)
#endif

#endif // KOS_COMMON_PANIC_HPP
