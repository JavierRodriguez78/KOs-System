#include <lib/elfloader.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>

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
    if (!image || size < sizeof(Elf32_Ehdr)) return false;
    const Elf32_Ehdr* eh = (const Elf32_Ehdr*)image;
    if (!(eh->e_ident[0] == 0x7F && eh->e_ident[1] == 'E' && eh->e_ident[2] == 'L' && eh->e_ident[3] == 'F')) return false;
    if (eh->e_machine != EM_386) return false;
    if (eh->e_phoff + eh->e_phnum * sizeof(Elf32_Phdr) > size) return false;
    if (eh->e_phentsize != sizeof(Elf32_Phdr)) return false;    // Load PT_LOAD segments by memcpy into their p_vaddr
    const Elf32_Phdr* ph = (const Elf32_Phdr*)(image + eh->e_phoff);
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_offset + ph[i].p_filesz > size) return false;
        uint8_t* dst = (uint8_t*)ph[i].p_vaddr;
        const uint8_t* src = image + ph[i].p_offset;
        // Copy file-backed segment into memory
        String::memmove(dst, src, ph[i].p_filesz);
        // Zero BSS tail (if any)
        if (ph[i].p_memsz > ph[i].p_filesz) {
            String::memset(dst + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
        }
    }
    int (*entry)() = (int (*)())(eh->e_entry);
    // Transfer control to program entry and execute
    if (entry) {
        (void)entry();
    }
    return true;
}