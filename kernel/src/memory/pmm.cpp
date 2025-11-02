#include <memory/pmm.hpp>
#include <console/logger.hpp>
#include <common/panic.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::memory;



// Simple static bitmap: supports up to 512 MiB by default (131072 frames)
// You can enlarge if needed. For now, detect based on memUpper and cap.
static uint32_t g_totalFrames = 0;
static uint32_t g_freeFrames = 0;

static const uint32_t MAX_FRAMES = 1024 * 1024; // up to 4 GiB / 4KiB = 1,048,576 frames; cap to 1M
static uint32_t g_frameCap = 0; // frames managed

static uint32_t g_bitmapWords = 0;
static uint32_t* g_bitmap = nullptr;

// Reserve space for a reasonably large bitmap (1,048,576 frames -> 1,048,576 bits -> 131,072 bytes -> 32,768 uint32)
// We'll put a fixed array in BSS and use only what we need.
static uint32_t s_bitmap_storage[32768];

static inline void bset(uint32_t idx) { g_bitmap[idx >> 5] |= (1u << (idx & 31)); }
static inline void bclr(uint32_t idx) { g_bitmap[idx >> 5] &= ~(1u << (idx & 31)); }
static inline bool bget(uint32_t idx) { return (g_bitmap[idx >> 5] >> (idx & 31)) & 1u; }

uint32_t PMM::TotalFrames() { return g_frameCap; }
uint32_t PMM::FreeFrames() { return g_freeFrames; }

void PMM::Init(uint32_t memLowerKB, uint32_t memUpperKB,
               phys_addr_t kernelStart, phys_addr_t kernelEnd,
               const void* multibootInfo)
{
    // memLowerKB: conventional memory below 1MiB
    // memUpperKB: amount of memory above 1MiB in KiB reported by Multiboot v1 (not accurate for > 4GiB)
    uint64_t totalBytes = (uint64_t)memLowerKB * 1024ull + (uint64_t)memUpperKB * 1024ull;
    if (memLowerKB == 0 && memUpperKB == 0) {
        // Fallback if multiboot memory not provided: assume 128 MiB total
        totalBytes = 128ull * 1024ull * 1024ull;
    }
    if (totalBytes < (1ull << 20)) totalBytes = (1ull << 20); // at least 1MiB

    g_totalFrames = (uint32_t)(totalBytes / PAGE_SIZE);
    if (g_totalFrames > MAX_FRAMES) g_totalFrames = MAX_FRAMES;
    g_frameCap = g_totalFrames;

    g_bitmap = s_bitmap_storage;
    g_bitmapWords = ((g_frameCap + 31) / 32);
    // clear bitmap: 0 = free
    for (uint32_t i = 0; i < g_bitmapWords; ++i) g_bitmap[i] = 0;

    // Reserve entire region below 1MB (0x100000) for BIOS/IVT/VGA/ROM
    // This includes 0x00000-0x9FFFF (conventional memory used by BIOS)
    // and 0xA0000-0xFFFFF (VGA memory, BIOS ROM, etc.)
    uint32_t reserveFrames = (0x100000u >> PAGE_SIZE_SHIFT);  // 256 frames = 1MB
    for (uint32_t f = 0; f < reserveFrames; ++f) {
        bset(f);
    }

    // Reserve kernel region
    uint32_t kstartF = (uint32_t)(kernelStart / PAGE_SIZE);
    uint32_t kendF = (uint32_t)((kernelEnd + PAGE_SIZE - 1) / PAGE_SIZE);
    if (kendF > g_frameCap) kendF = g_frameCap;
    for (uint32_t f = kstartF; f < kendF; ++f) bset(f);

    // Optionally, we could parse Multiboot memory map to free/reserve specific regions.
    // For now, assume everything from 1MiB..totalBytes is free except kernel.
    // This ensures we only allocate frames from actual RAM above 1MB.

    // Count free frames
    uint32_t used = 0;
    for (uint32_t f = 0; f < g_frameCap; ++f) if (bget(f)) ++used;
    g_freeFrames = g_frameCap - used;

    char msg[80];
    // Simple decimal formatter
    auto utoa = [](uint32_t v, char* b){ char tmp[16]; int i=0; if(!v){b[0]='0';b[1]=0;return;} while(v){tmp[i++]='0'+(v%10);v/=10;} int j=0; while(i){b[j++]=tmp[--i];} b[j]=0; };
    char tf[16], ff[16]; utoa(g_frameCap, tf); utoa(g_freeFrames, ff);
    const char* prefix = "PMM: frames total=";
    int k = 0; for (const char* p = prefix; *p; ++p) msg[k++] = *p; for (int i=0; tf[i]; ++i) msg[k++]=tf[i]; msg[k++]=' ';
    const char* mid = "free="; for (const char* p = mid; *p; ++p) msg[k++] = *p; for (int i=0; ff[i]; ++i) msg[k++]=ff[i]; msg[k]=0;
    Logger::Log(msg);
}

void PMM::markRangeUsed(phys_addr_t start, phys_addr_t end)
{
    if (end <= start) return;
    uint32_t s = (uint32_t)(start / PAGE_SIZE);
    uint32_t e = (uint32_t)((end + PAGE_SIZE - 1) / PAGE_SIZE);
    if (e > g_frameCap) e = g_frameCap;
    for (uint32_t f = s; f < e; ++f) if (!bget(f)) { bset(f); if (g_freeFrames) --g_freeFrames; }
}

void PMM::markRangeFree(phys_addr_t start, phys_addr_t end)
{
    if (end <= start) return;
    uint32_t s = (uint32_t)(start / PAGE_SIZE);
    uint32_t e = (uint32_t)((end + PAGE_SIZE - 1) / PAGE_SIZE);
    if (e > g_frameCap) e = g_frameCap;
    for (uint32_t f = s; f < e; ++f) if (bget(f)) { bclr(f); ++g_freeFrames; }
}

phys_addr_t PMM::AllocFrame()
{
    // First-fit
    for (uint32_t f = 0; f < g_frameCap; ++f) {
        if (!bget(f)) {
            bset(f);
            if (g_freeFrames) --g_freeFrames;
            phys_addr_t pa = (phys_addr_t)f * PAGE_SIZE;
            // Invariant: PMM allocations must be page-aligned
            KASSERT((pa & (PAGE_SIZE - 1)) == 0);
            return pa;
        }
    }
    return 0;
}

void PMM::FreeFrame(phys_addr_t addr)
{
    // Invariant: freeing must be page-aligned
    KASSERT(((uint32_t)addr & (PAGE_SIZE - 1)) == 0);
    uint32_t f = (uint32_t)(addr / PAGE_SIZE);
    if (f < g_frameCap && bget(f)) { bclr(f); ++g_freeFrames; }
}


