#include <memory/heap.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>
#include <console/logger.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::memory;

static virt_addr_t g_heapBase = 0;
static virt_addr_t g_heapBrk = 0;  // next free address
static virt_addr_t g_heapEnd = 0;  // mapped end

static inline uint32_t align_up(uint32_t v, uint32_t a) { 
    return (v + a - 1) & ~(a - 1); 
}

void Heap::Init(virt_addr_t base, uint32_t initialPages)
{
    g_heapBase = base;
    g_heapBrk = base;
    g_heapEnd = base;
    if (initialPages) {
        ensure(base + initialPages * PAGE_SIZE);
    }
    Logger::Log("Kernel heap initialized");
}

bool Heap::ensure(virt_addr_t upto)
{
    while (g_heapEnd < upto) {
        phys_addr_t frame = PMM::AllocFrame();
        if (!frame) return false;
        Paging::MapPage(g_heapEnd, frame, Paging::Present | Paging::RW);
        g_heapEnd += PAGE_SIZE;
    }
    return true;
}

void* Heap::Alloc(uint32_t size, uint32_t align)
{
    if (align < 8) align = 8;
    virt_addr_t cur = align_up((uint32_t)g_heapBrk, align);
    virt_addr_t need = cur + size;
    if (!ensure(need)) return 0;
    g_heapBrk = need;
    return (void*)cur;
}

void Heap::Free(void* ) { /* no-op */ }

virt_addr_t Heap::Brk() { return g_heapBrk; }
virt_addr_t Heap::End() { return g_heapEnd; }
