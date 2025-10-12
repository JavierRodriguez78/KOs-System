// Minimal global new/delete operators for freestanding kernel
// This version is for user apps and early builds; returns null/no-op.

#include <common/types.hpp>

using namespace kos::common;
// No kernel heap here (apps are freestanding C); keep stubs.

// Regular new/delete
void* operator new(uint32_t) noexcept { return 0; }

void operator delete(void* ) noexcept {
    // no-op
}

// Sized delete (GCC emits this in C++14 for deleting destructors)
void operator delete(void* , uint32_t) noexcept {
    // no-op
}

// Array new/delete
void* operator new[](uint32_t) noexcept { return 0; }

void operator delete[](void*) noexcept {
    // no-op
}

void operator delete[](void*, uint32_t) noexcept {
    // no-op
}

// Optional: pure virtual call handler to avoid unresolved if ever hit
extern "C" void __cxa_pure_virtual() {
    // Hang or ignore
    while (1) { }
}
