#include <memory/heap.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>
#include <console/logger.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::memory;

static virt_addr_t g_heapBase = 0;
static virt_addr_t g_heapEnd = 0;  // mapped end
static uint32_t g_heapUsed = 0;
static volatile uint32_t g_heapLock = 0;

namespace {

struct BlockHeader {
    uint32_t magic;
    uint32_t size;
    BlockHeader* prev;
    BlockHeader* next;
    uint32_t used;
    uint32_t reserved;
};

static constexpr uint32_t HEAP_MAGIC = 0x48454150u; // HEAP
static constexpr uint32_t MIN_ALIGNMENT = 8;
static constexpr uint32_t SPLIT_THRESHOLD = 32;

static BlockHeader* g_firstBlock = nullptr;
static BlockHeader* g_lastBlock = nullptr;

static inline uintptr_t align_up_ptr(uintptr_t value, uint32_t align) {
    return (value + align - 1u) & ~(uintptr_t)(align - 1u);
}

static inline uint32_t normalize_align(uint32_t align) {
    if (align < MIN_ALIGNMENT) align = MIN_ALIGNMENT;
    if ((align & (align - 1u)) != 0) {
        uint32_t normalized = MIN_ALIGNMENT;
        while (normalized < align) normalized <<= 1u;
        align = normalized;
    }
    return align;
}

static inline uint8_t* payload_start(BlockHeader* block) {
    return reinterpret_cast<uint8_t*>(block) + sizeof(BlockHeader);
}

static inline BlockHeader* block_from_payload(void* ptr) {
    return ptr ? *(reinterpret_cast<BlockHeader**>(ptr) - 1) : nullptr;
}

static inline void lock_heap() {
    while (__sync_lock_test_and_set(&g_heapLock, 1u) != 0u) {
    }
}

static inline void unlock_heap() {
    __sync_lock_release(&g_heapLock);
}

static BlockHeader* create_block_at(virt_addr_t addr, uint32_t size, BlockHeader* prev) {
    BlockHeader* block = reinterpret_cast<BlockHeader*>(addr);
    block->magic = HEAP_MAGIC;
    block->size = size;
    block->prev = prev;
    block->next = nullptr;
    block->used = 0;
    block->reserved = 0;
    if (prev) prev->next = block;
    if (!g_firstBlock) g_firstBlock = block;
    g_lastBlock = block;
    return block;
}

static void absorb_next(BlockHeader* block) {
    BlockHeader* next = block ? block->next : nullptr;
    if (!block || !next || next->used) return;
    block->size += (uint32_t)sizeof(BlockHeader) + next->size;
    block->next = next->next;
    if (block->next) block->next->prev = block;
    else g_lastBlock = block;
}

static void split_block(BlockHeader* block, uint32_t usedSize) {
    if (!block) return;
    if (block->size <= usedSize + sizeof(BlockHeader) + SPLIT_THRESHOLD) return;

    uint8_t* newAddr = payload_start(block) + usedSize;
    uint32_t remaining = block->size - usedSize - (uint32_t)sizeof(BlockHeader);
    BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(newAddr);
    newBlock->magic = HEAP_MAGIC;
    newBlock->size = remaining;
    newBlock->prev = block;
    newBlock->next = block->next;
    newBlock->used = 0;
    newBlock->reserved = 0;
    if (newBlock->next) newBlock->next->prev = newBlock;
    else g_lastBlock = newBlock;
    block->next = newBlock;
    block->size = usedSize;
}

static bool grow_heap(uint32_t minPayload) {
    uint32_t totalBytes = minPayload + (uint32_t)sizeof(BlockHeader);
    uint32_t pages = (totalBytes + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (pages == 0) pages = 1;

    virt_addr_t growStart = g_heapEnd;
    for (uint32_t i = 0; i < pages; ++i) {
        phys_addr_t frame = PMM::AllocFrame();
        if (!frame) return false;
        Paging::MapPage(g_heapEnd, frame, Paging::Present | Paging::RW);
        g_heapEnd += PAGE_SIZE;
    }

    uint32_t addedBytes = (uint32_t)(g_heapEnd - growStart);
    if (g_lastBlock && !g_lastBlock->used) {
        g_lastBlock->size += addedBytes;
        return true;
    }

    create_block_at(growStart, addedBytes - (uint32_t)sizeof(BlockHeader), g_lastBlock);
    return true;
}

static BlockHeader* find_block(uint32_t size, uint32_t align, uintptr_t* userPtrOut, uint32_t* consumedOut) {
    for (BlockHeader* block = g_firstBlock; block; block = block->next) {
        if (block->used) continue;
        uintptr_t raw = reinterpret_cast<uintptr_t>(payload_start(block));
        uintptr_t user = align_up_ptr(raw + sizeof(BlockHeader*), align);
        uint32_t prefix = (uint32_t)(user - raw);
        uint32_t consumed = prefix + size;
        if (consumed > block->size) continue;
        *userPtrOut = user;
        *consumedOut = consumed;
        return block;
    }
    return nullptr;
}

} // namespace

void Heap::Init(virt_addr_t base, uint32_t initialPages)
{
    g_heapBase = base;
    g_heapEnd = base;
    g_heapUsed = 0;
    g_firstBlock = nullptr;
    g_lastBlock = nullptr;
    if (initialPages == 0) initialPages = 1;
    if (ensure(base + initialPages * PAGE_SIZE)) {
        create_block_at(base, (uint32_t)(g_heapEnd - g_heapBase) - (uint32_t)sizeof(BlockHeader), nullptr);
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
    if (size == 0) return 0;
    align = normalize_align(align);

    lock_heap();

    uintptr_t userPtr = 0;
    uint32_t consumed = 0;
    BlockHeader* block = find_block(size, align, &userPtr, &consumed);
    if (!block) {
        if (!grow_heap(size + align + (uint32_t)sizeof(BlockHeader*))) {
            unlock_heap();
            return 0;
        }
        block = find_block(size, align, &userPtr, &consumed);
        if (!block) {
            unlock_heap();
            return 0;
        }
    }

    split_block(block, consumed);
    block->used = 1;
    *(reinterpret_cast<BlockHeader**>(userPtr) - 1) = block;
    g_heapUsed += block->size;

    unlock_heap();
    return reinterpret_cast<void*>(userPtr);
}

void Heap::Free(void* ptr) {
    if (!ptr) return;

    lock_heap();

    BlockHeader* block = block_from_payload(ptr);
    if (!block || block->magic != HEAP_MAGIC || !block->used) {
        unlock_heap();
        return;
    }

    block->used = 0;
    if (g_heapUsed >= block->size) g_heapUsed -= block->size;
    else g_heapUsed = 0;

    absorb_next(block);
    if (block->prev && !block->prev->used) {
        absorb_next(block->prev);
    }

    unlock_heap();
}

uint32_t Heap::Used() { return g_heapUsed; }

virt_addr_t Heap::Brk() { return g_heapBase + g_heapUsed; }
virt_addr_t Heap::End() { return g_heapEnd; }
