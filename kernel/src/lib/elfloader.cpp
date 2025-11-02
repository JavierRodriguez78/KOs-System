#include <lib/elfloader.hpp>
#include <console/tty.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>
#include <common/panic.hpp>

using namespace kos::lib;
using namespace kos::common;
using namespace kos::console;
using namespace kos::memory;

// Minimal ELF32 definitions
struct Elf32_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

static const uint32_t PT_LOAD = 1;
static const uint16_t EM_386 = 3;

bool ELFLoader::LoadAndExecute(const uint8_t* image, uint32_t size) {
    if (!image || size < sizeof(Elf32_Ehdr)) { TTY::Write((int8_t*)"ELF: too small\n"); return false; }
    const Elf32_Ehdr* eh = (const Elf32_Ehdr*)image;
    if (!(eh->e_ident[0] == 0x7F && eh->e_ident[1] == 'E' && eh->e_ident[2] == 'L' && eh->e_ident[3] == 'F')) { TTY::Write((int8_t*)"ELF: bad magic\n"); return false; }
    // e_ident[4] = EI_CLASS (1=ELF32), e_ident[5] = EI_DATA (1=little-endian), e_ident[6] = EI_VERSION (1)
    if (eh->e_ident[4] != 1) { TTY::Write((int8_t*)"ELF: not ELF32\n"); return false; }
    if (eh->e_ident[5] != 1) { TTY::Write((int8_t*)"ELF: not little-endian\n"); return false; }
    if (eh->e_ident[6] != 1) { TTY::Write((int8_t*)"ELF: bad ident version\n"); return false; }
    if (eh->e_machine != EM_386) { TTY::Write((int8_t*)"ELF: not i386\n"); return false; }
    uint32_t need = eh->e_phoff + eh->e_phnum * sizeof(Elf32_Phdr);
    if (need < eh->e_phoff || need > size) { TTY::Write((int8_t*)"ELF: phdr table out of range\n"); return false; }
    if (eh->e_phentsize != sizeof(Elf32_Phdr)) { TTY::Write((int8_t*)"ELF: phentsize mismatch\n"); return false; }
    // Load PT_LOAD segments by memcpy into their p_vaddr
    const Elf32_Phdr* ph = (const Elf32_Phdr*)(image + eh->e_phoff);
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_offset + ph[i].p_filesz > size) { TTY::Write((int8_t*)"ELF: segment exceeds image\n"); return false; }
        uint8_t* dst = (uint8_t*)ph[i].p_vaddr;
        const uint8_t* src = image + ph[i].p_offset;
        // Map pages for the load segment if not already mapped (identity maps may not cover app address)
        uint32_t segSize = ph[i].p_memsz;
        uint32_t vaddr = ph[i].p_vaddr;
        uint32_t vpage = vaddr & 0xFFFFF000;
        uint32_t pageOffset = vaddr & 0xFFF;
        uint32_t total = pageOffset + segSize;
        uint32_t mapSize = (total + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint32_t pa;
        // For now, allocate fresh frames for the app segment; a more advanced loader could honor p_paddr
        for (uint32_t off = 0; off < mapSize; off += PAGE_SIZE) {
            // Proactively unmap any existing identity mappings for this page
            Paging::UnmapPage(vpage + off);
            pa = PMM::AllocFrame();
            if (!pa) { TTY::Write((int8_t*)"ELF: OOM frames\n"); return false; }
            // Kernel invariant: PMM must return page-aligned frames
            KASSERT((pa & (PAGE_SIZE - 1)) == 0);
            
            if (Logger::IsDebugEnabled()) {
                // Debug: Check if physical frame is in valid range
                TTY::Write((int8_t*)"Mapping VA=");
                for (int k = 7; k >= 0; --k) {
                    uint8_t nyb = ((vpage + off) >> (k*4)) & 0xF;
                    TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
                }
                TTY::Write((int8_t*)" PA=");
                for (int k = 7; k >= 0; --k) {
                    uint8_t nyb = (pa >> (k*4)) & 0xF;
                    TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
                }
                
                // Check if PA is in identity-mapped region
                if (pa >= 64*1024*1024) {
                    TTY::Write((int8_t*)" PA_TOO_HIGH!");
                }
            }
            
            kos::memory::Paging::MapPage(vpage + off, pa, kos::memory::Paging::Present | kos::memory::Paging::RW | kos::memory::Paging::User);
            
            // Force TLB flush for this specific page
            asm volatile("invlpg (%0)" :: "r"(vpage + off) : "memory");
            
            if (Logger::IsDebugEnabled()) {
                // Test if mapping succeeded by checking GetPhys
                phys_addr_t testPA = kos::memory::Paging::GetPhys(vpage + off);
                TTY::Write((int8_t*)" GetPhys=");
                for (int k = 7; k >= 0; --k) {
                    uint8_t nyb = (testPA >> (k*4)) & 0xF;
                    TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
                }
                
                if (testPA == 0) {
                    TTY::Write((int8_t*)" MAPPING FAILED!\n");
                    return false;
                }
                
                // First test: try writing directly to physical address via identity mapping
                TTY::Write((int8_t*)" phys_test:");
                if (pa < 64*1024*1024) {
                    uint8_t* physPtr = (uint8_t*)pa;
                    physPtr[0] = 0x11; physPtr[1] = 0x22; physPtr[2] = 0x33; physPtr[3] = 0x44;
                    TTY::WriteHex(physPtr[0]); TTY::WriteHex(physPtr[1]); TTY::WriteHex(physPtr[2]); TTY::WriteHex(physPtr[3]);
                } else {
                    TTY::Write((int8_t*)"N/A");
                }
                
                // Test the mapping by writing and reading back with more detail
                uint8_t* pagePtr = (uint8_t*)(vpage + off);
                pagePtr[0] = 0xAA; pagePtr[1] = 0xBB; pagePtr[2] = 0xCC; pagePtr[3] = 0xDD;
                TTY::Write((int8_t*)" virt_test:");
                TTY::WriteHex(pagePtr[0]); TTY::WriteHex(pagePtr[1]); TTY::WriteHex(pagePtr[2]); TTY::WriteHex(pagePtr[3]);
                
                if (pagePtr[0] != 0xAA || pagePtr[1] != 0xBB || pagePtr[2] != 0xCC || pagePtr[3] != 0xDD) {
                    TTY::Write((int8_t*)" VIRT_FAIL!");
                }
                
                TTY::Write((int8_t*)" OK\n");
            }
            
            // Zero the newly mapped page to ensure clean state
            uint8_t* pagePtr = (uint8_t*)(vpage + off);
            for (int j = 0; j < PAGE_SIZE; j++) pagePtr[j] = 0;
        }
        // Invariant: mapping must be present for the destination address now
        KASSERT(kos::memory::Paging::GetPhys((uintptr_t)dst) != 0);
        
        // Flush TLB to ensure mappings are visible before copying
        kos::memory::Paging::FlushAll();
        
        if (Logger::IsDebugEnabled()) {
            // Test if we can write to the mapped memory at all
            TTY::Write((int8_t*)"Testing write to ");
            for (int j = 7; j >= 0; --j) {
                uint8_t nyb = (ph[i].p_vaddr >> (j*4)) & 0xF;
                TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
            }
            TTY::Write((int8_t*)": ");
            
            dst[0] = 0xDE; dst[1] = 0xAD; dst[2] = 0xBE; dst[3] = 0xEF;
            TTY::WriteHex(dst[0]); TTY::WriteHex(dst[1]); TTY::WriteHex(dst[2]); TTY::WriteHex(dst[3]);
        }
        
        // Copy file-backed segment into memory
        String::memmove(dst, src, ph[i].p_filesz);
        // Zero BSS tail (if any)
        if (ph[i].p_memsz > ph[i].p_filesz) {
            String::memset(dst + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
        }
        
        if (Logger::IsDebugEnabled()) {
            // Debug: verify the copy worked by checking first few bytes
            TTY::Write((int8_t*)" -> copied: ");
            for (int j = 0; j < 4; ++j) {
                TTY::PutChar("0123456789ABCDEF"[(dst[j]>>4)&0xF]);
                TTY::PutChar("0123456789ABCDEF"[dst[j]&0xF]);
                TTY::PutChar(' ');
            }
            TTY::PutChar('\n');
        }
        // If segment is not writable (p_flags bit 1), drop RW from mapped pages
        const uint32_t PF_W = 0x2;
        if ((ph[i].p_flags & PF_W) == 0) {
            uint32_t vaddr = ph[i].p_vaddr;
            uint32_t vpage = vaddr & 0xFFFFF000;
            uint32_t pageOffset = vaddr & 0xFFF;
            uint32_t total = pageOffset + ph[i].p_memsz;
            uint32_t mapSize = (total + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            for (uint32_t off = 0; off < mapSize; off += PAGE_SIZE) {
                Paging::RemapPageFlags(vpage + off, Paging::Present | Paging::User);
            }
        }
    }
    // Validate entry lies inside a PT_LOAD range we just mapped
    uint32_t entryVA = eh->e_entry;
    bool entryOK = false;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint32_t start = ph[i].p_vaddr;
        uint32_t end = ph[i].p_vaddr + (ph[i].p_memsz ? ph[i].p_memsz : ph[i].p_filesz);
        if (entryVA >= start && entryVA < end) { entryOK = true; break; }
    }
    if (!entryOK) { TTY::Write((int8_t*)"ELF: entry not in PT_LOAD\n"); return false; }

    // Ensure all new mappings are visible to the CPU
    Paging::FlushAll();

    if (Logger::IsDebugEnabled()) {
        // Debug: print entry, phys mapping, dst bytes and src bytes
        TTY::Write((int8_t*)"ELF entry=");
        uint32_t v = entryVA;
        TTY::Write((int8_t*)"0x");
        for (int i = 7; i >= 0; --i) {
            uint8_t nyb = (v >> (i*4)) & 0xF;
            TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
        }
        TTY::Write((int8_t*)" phys=");
        uint32_t pa = Paging::GetPhys((uintptr_t)entryVA);
        TTY::Write((int8_t*)"0x");
        for (int i = 7; i >= 0; --i) {
            uint8_t nyb = (pa >> (i*4)) & 0xF;
            TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
        }
        
        // Debug: Check if the issue is that our copy never happened
        TTY::Write((int8_t*)" test_write:");
        uint8_t* testPtr = (uint8_t*)entryVA;
        uint8_t oldVal = testPtr[0];
        testPtr[0] = 0xAA;  // Write test pattern
        TTY::WriteHex(testPtr[0]); // Should show AA if writable
        testPtr[0] = oldVal;  // Restore
        TTY::Write((int8_t*)" dst:");
        uint8_t* p = (uint8_t*)entryVA;
        for (int i = 0; i < 8; ++i) {
            TTY::PutChar(' ');
            TTY::PutChar("0123456789ABCDEF"[(p[i]>>4)&0xF]);
            TTY::PutChar("0123456789ABCDEF"[p[i]&0xF]);
        }
        // find source bytes from ELF image for comparison
        const uint8_t* srcBytes = 0;
        for (uint16_t i = 0; i < eh->e_phnum; ++i) {
            if (ph[i].p_type != PT_LOAD) continue;
            if (entryVA >= ph[i].p_vaddr && entryVA < ph[i].p_vaddr + (ph[i].p_filesz ? ph[i].p_filesz : ph[i].p_memsz)) {
                uint32_t off = entryVA - ph[i].p_vaddr;
                if (off < ph[i].p_filesz) srcBytes = image + ph[i].p_offset + off;
                break;
            }
        }
        if (srcBytes) {
            TTY::Write((int8_t*)" src:");
            for (int i = 0; i < 8; ++i) {
                TTY::PutChar(' ');
                TTY::PutChar("0123456789ABCDEF"[(srcBytes[i]>>4)&0xF]);
                TTY::PutChar("0123456789ABCDEF"[srcBytes[i]&0xF]);
            }
        }
        
        // Additional debug: check if phys frame looks like it contains expected data
        TTY::Write((int8_t*)" phys_direct:");
        if (pa && pa < 64*1024*1024) {  // within identity mapped region
            uint8_t* physDirect = (uint8_t*)pa;
            for (int i = 0; i < 8; ++i) {
                TTY::PutChar(' ');
                TTY::PutChar("0123456789ABCDEF"[(physDirect[i]>>4)&0xF]);
                TTY::PutChar("0123456789ABCDEF"[physDirect[i]&0xF]);
            }
        }
        TTY::PutChar('\n');
        
        // PAUSE: Wait for user input to continue after diagnostic
        TTY::Write((int8_t*)"Press any key to continue...\n");
        // Simple wait loop - halt until keyboard interrupt
        // asm volatile("hlt");
    }

    int (*entry)() = (int (*)())entryVA;
    // Transfer control to program entry and execute
    if (entry) {
        (void)entry();
        return true;
    }
    TTY::Write((int8_t*)"ELF: null entry\n");
    return false;
}