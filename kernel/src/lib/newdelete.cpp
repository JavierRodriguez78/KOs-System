// Minimal global new/delete operators for freestanding kernel
// Avoid dynamic allocation; these are stubs to satisfy the linker.

#include <common/types.hpp>

using namespace kos::common;

// Regular new/delete
void* operator new(uint32_t) {
    return 0; // no heap; return null
}

void operator delete(void* ) noexcept {
    // no-op
}

// Sized delete (GCC emits this in C++14 for deleting destructors)
void operator delete(void* , uint32_t) noexcept {
    // no-op
}

// Array new/delete
void* operator new[](uint32_t) {
    return 0;
}

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
