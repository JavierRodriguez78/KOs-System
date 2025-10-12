#pragma once
#ifndef __KOS__MEMORY__PAGING_H
#define __KOS__MEMORY__PAGING_H

#include <common/types.hpp>
#include <memory/memory.hpp>

namespace kos { 
    namespace memory {
        class Paging {
            public:
                // Setup initial page directory/tables and enable paging (identity maps low memory and kernel)
                static void Init(phys_addr_t kernelStart, phys_addr_t kernelEnd);

                // Map a single 4KiB page: virt -> phys with flags
                static void MapPage(virt_addr_t vaddr, phys_addr_t paddr, uint32_t flags);
                static void UnmapPage(virt_addr_t vaddr);
                static phys_addr_t GetPhys(virt_addr_t vaddr);
                static void UnmapRange(virt_addr_t vaddr, uint32_t size);
                static void FlushAll();

            // Convenience helpers
            static void MapRange(virt_addr_t vaddr, phys_addr_t paddr, uint32_t size, uint32_t flags);
            static void RemapPageFlags(virt_addr_t vaddr, uint32_t flags);
            static void RemapRangeFlags(virt_addr_t vaddr, uint32_t size, uint32_t flags);

                // Flags similar to x86: present(1), rw(2), user(4), write-through(8), cache-disable(16), accessed(32)
                enum Flags { Present=1, RW=2, User=4, WriteThrough=8, CacheDisable=16, Accessed=32 }; 
        };      

    }
}

#endif