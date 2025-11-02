#include <memory/paging.hpp>
#include <memory/pmm.hpp>
#include <console/logger.hpp>
#include <common/panic.hpp>

// Linker-provided section boundary symbols (global C linkage)
extern "C" {
    extern uint8_t kernel_start, kernel_end;
    extern uint8_t text_end, rodata_start, rodata_end, data_start, bss_start, bss_end;
}

using namespace kos::common;
using namespace kos::console;
using namespace kos::memory;



// x86 32-bit paging structures (not PAE)
struct PageDirectoryEntry { uint32_t value; } __attribute__((packed));
struct PageTableEntry { uint32_t value; } __attribute__((packed));

static PageDirectoryEntry* g_pageDirectory = nullptr; // must be page-aligned

static inline void invlpg(void* m) { asm volatile("invlpg (%0)" : : "r"(m) : "memory"); }

static inline void load_cr3(uint32_t phys) { asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory"); }
static inline uint32_t read_cr0() { uint32_t v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline void write_cr0(uint32_t v) { asm volatile("mov %0, %%cr0" : : "r"(v) : "memory"); }

static inline uint32_t pde_index(uint32_t addr) { return (addr >> 22) & 0x3FF; }
static inline uint32_t pte_index(uint32_t addr) { return (addr >> 12) & 0x3FF; }

static PageTableEntry* ensureTable(uint32_t vaddr, bool create, bool user=false)
{
    uint32_t pdi = pde_index(vaddr);
    if (!(g_pageDirectory[pdi].value & 1)) {
        if (!create) return nullptr;
        
        // CRITICAL FIX: Only allocate page table frames from identity-mapped region
        // We need to ensure we can access the page table after allocation
        phys_addr_t frame;
        const uint32_t MAX_TRIES = 100;  // Prevent infinite loops
        uint32_t tries = 0;
        
        do {
            frame = PMM::AllocFrame();
            if (frame == 0) return nullptr;  // Out of memory
            // Frame returned by PMM must be page-aligned
            KASSERT((frame & (PAGE_SIZE - 1)) == 0);
            
            // If frame is in identity-mapped region (< 64MB), we can use it
            if (frame < 64*1024*1024) {
                break;
            }
            
            // Frame is outside identity-mapped region, free it and try again
            PMM::FreeFrame(frame);
            tries++;
        } while (tries < MAX_TRIES);
        
        if (tries >= MAX_TRIES) {
            // Could not find a suitable frame in identity-mapped region
            return nullptr;
        }
        
    PageTableEntry* v = (PageTableEntry*)frame; // Safe: frame < 64MB, so identity-mapped
        for (int i = 0; i < 1024; ++i) v[i].value = 0;
        uint32_t pdeFlags = (Paging::Present | Paging::RW);
        if (user) pdeFlags |= Paging::User;
        g_pageDirectory[pdi].value = (frame & 0xFFFFF000) | pdeFlags;
        return v;
    }
    
    // Get existing page table - verify it's still accessible
    uint32_t ptPhys = g_pageDirectory[pdi].value & 0xFFFFF000;
    if (ptPhys >= 64*1024*1024) {
        // Page table is outside our identity mapped region - we can't access it!
        return nullptr;
    }
    return (PageTableEntry*)ptPhys;
}

void Paging::MapPage(virt_addr_t vaddr, phys_addr_t paddr, uint32_t flags)
{
    uint32_t va = (uint32_t)vaddr;
    // Invariants: physical address must be aligned
    KASSERT(((uint32_t)paddr & (PAGE_SIZE - 1)) == 0);
    uint32_t pdi = pde_index(va);
    PageTableEntry* pt = ensureTable(va, true, (flags & User) != 0);
    if (!pt) return;
    uint32_t pti = pte_index(va);
    // If mapping with User flag, ensure PDE also has User bit
    if (flags & User) {
        g_pageDirectory[pdi].value |= User;
    }
    pt[pti].value = (paddr & 0xFFFFF000) | (flags & 0xFFF) | Present;
    invlpg((void*)vaddr);
}

void Paging::UnmapPage(virt_addr_t vaddr)
{
    PageTableEntry* pt = ensureTable((uint32_t)vaddr, false);
    if (!pt) return;
    uint32_t pti = pte_index((uint32_t)vaddr);
    pt[pti].value = 0;
    invlpg((void*)vaddr);
}

void Paging::UnmapRange(virt_addr_t vaddr, uint32_t size)
{
    uint32_t off = 0;
    while (off < size) {
        UnmapPage(vaddr + off);
        off += PAGE_SIZE;
    }
}

phys_addr_t Paging::GetPhys(virt_addr_t vaddr)
{
    PageTableEntry* pt = ensureTable((uint32_t)vaddr, false);
    if (!pt) return 0;
    uint32_t pti = pte_index((uint32_t)vaddr);
    if (!(pt[pti].value & Present)) return 0;
    return (pt[pti].value & 0xFFFFF000) | ((uint32_t)vaddr & 0xFFF);
}

void Paging::MapRange(virt_addr_t vaddr, phys_addr_t paddr, uint32_t size, uint32_t flags)
{
    uint32_t off = 0;
    while (off < size) {
        MapPage(vaddr + off, paddr + off, flags);
        off += PAGE_SIZE;
    }
}

void Paging::RemapPageFlags(virt_addr_t vaddr, uint32_t flags)
{
    PageTableEntry* pt = ensureTable((uint32_t)vaddr, (flags & User) != 0, (flags & User) != 0);
    if (!pt) return;
    uint32_t pti = pte_index((uint32_t)vaddr);
    if (!(pt[pti].value & Present)) return;
    uint32_t phys = pt[pti].value & 0xFFFFF000;
    pt[pti].value = phys | (flags & 0xFFF) | Present;
    invlpg((void*)vaddr);
}

void Paging::RemapRangeFlags(virt_addr_t vaddr, uint32_t size, uint32_t flags)
{
    uint32_t off = 0;
    while (off < size) {
        RemapPageFlags(vaddr + off, flags);
        off += PAGE_SIZE;
    }
}

void Paging::FlushAll()
{
    // Reload CR3 to flush TLB
    asm volatile("mov %%cr3, %%eax\n\tmov %%eax, %%cr3" ::: "eax", "memory");
}

void Paging::Init(phys_addr_t kernelStart, phys_addr_t kernelEnd)
{
    // Allocate one frame for page directory
    phys_addr_t pdPhys = PMM::AllocFrame();
    if (!pdPhys) { Logger::Log("Paging: failed to allocate page directory"); return; }
    g_pageDirectory = (PageDirectoryEntry*)pdPhys; // identity map is active until we enable paging
    // clear directory
    for (int i = 0; i < 1024; ++i) g_pageDirectory[i].value = 0;

    // Identity map the first 64 MiB to be safe for device/DMA and early allocations
    const uint32_t ID_MAP_END = 64 * 1024 * 1024;
    for (uint32_t addr = 0; addr < ID_MAP_END; addr += PAGE_SIZE) {
        MapPage(addr, addr, Present | RW);
    }

    // Ensure kernel range is mapped (it already is, due to identity maps)
    // Load CR3 with PD physical address
    load_cr3((uint32_t)pdPhys);

    // Enable paging bit (CR0.PG = 1)
    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000u; // PG
    write_cr0(cr0);

    Logger::Log("Paging enabled (32-bit)");

    // After paging is on, set page protections for kernel sections if they are identity mapped
    extern uint8_t text_end, rodata_start, rodata_end, data_start;
    // Make .text read+execute (RW cleared). On x86 without NX, we can only drop write.
    RemapRangeFlags((virt_addr_t)0x00100000, (virt_addr_t)&::text_end - (virt_addr_t)0x00100000, Present /*no RW*/);
    // Make .rodata read-only
    RemapRangeFlags((virt_addr_t)&::rodata_start, (uint32_t)((virt_addr_t)&::rodata_end - (virt_addr_t)&::rodata_start), Present /*no RW*/);

    // NOTE: Don't clear identity mappings yet - our page tables might be allocated there
    // and we need to access them. We'll handle this in the ELF loader instead.
    // UnmapRange((virt_addr_t)0x01000000, 4 * 1024 * 1024);
}

