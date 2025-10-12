#include <lib/elfloader.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>

using namespace kos::lib;
using namespace kos::common;
using namespace kos::console;

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
            kos::memory::Paging::UnmapPage(vpage + off);
            pa = kos::memory::PMM::AllocFrame();
            if (!pa) { TTY::Write((int8_t*)"ELF: OOM frames\n"); return false; }
            kos::memory::Paging::MapPage(vpage + off, pa, kos::memory::Paging::Present | kos::memory::Paging::RW | kos::memory::Paging::User);
        }
        // Copy file-backed segment into memory
        String::memmove(dst, src, ph[i].p_filesz);
        // Zero BSS tail (if any)
        if (ph[i].p_memsz > ph[i].p_filesz) {
            String::memset(dst + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
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
                kos::memory::Paging::RemapPageFlags(vpage + off, kos::memory::Paging::Present | kos::memory::Paging::User);
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
    kos::memory::Paging::FlushAll();

    // Debug: print entry, phys mapping, dst bytes and src bytes
    TTY::Write((int8_t*)"ELF entry=");
    {
        uint32_t v = entryVA;
        TTY::Write((int8_t*)"0x");
        for (int i = 7; i >= 0; --i) {
            uint8_t nyb = (v >> (i*4)) & 0xF;
            TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
        }
        TTY::Write((int8_t*)" phys=");
        uint32_t pa = kos::memory::Paging::GetPhys((kos::common::uintptr_t)entryVA);
        TTY::Write((int8_t*)"0x");
        for (int i = 7; i >= 0; --i) {
            uint8_t nyb = (pa >> (i*4)) & 0xF;
            TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
        }
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
        TTY::PutChar('\n');
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