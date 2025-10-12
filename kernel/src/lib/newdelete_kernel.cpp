// Kernel-only global new/delete wired to kernel heap
#include <common/types.hpp>
#include <memory/heap.hpp>

using namespace kos::common;

void* operator new(uint32_t sz) {
    return kos::memory::Heap::Alloc(sz, 8);
}

void operator delete(void* ) noexcept {}
void operator delete(void* , uint32_t) noexcept {}

void* operator new[](uint32_t sz) {
    return kos::memory::Heap::Alloc(sz, 8);
}

void operator delete[](void*) noexcept {}
void operator delete[](void*, uint32_t) noexcept {}

extern "C" void __cxa_pure_virtual() { while (1) { } }
