#include <stddef.h>

namespace kos { namespace console { void LogToJournal(const char* msg); }}

// C ABI wrapper so C apps can call LogToJournal
extern "C" void LogToJournal(const char* message) {
    // Forward to the kernel-side (or stub) implementation
    kos::console::LogToJournal(message ? message : "");
}
