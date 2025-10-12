#pragma once
#ifndef __KOS__MEMORY__PMM_H
#define __KOS__MEMORY__PMM_H

#include <common/types.hpp>
#include <memory/memory.hpp>

namespace kos { 
    namespace memory {

        // Very simple bitmap-based physical frame allocator
        // Frames are PAGE_SIZE (4KiB). The bitmap itself is stored in the .bss.
        class PMM {
            public:
    
                // Initialize allocator with total memory size in bytes and the memory map from multiboot (optional)
                // If mmap is null or not used, we assume a contiguous region [1MiB, totalBytes)
                static void Init(uint32_t memLowerKB, uint32_t memUpperKB,
                     phys_addr_t kernelStart, phys_addr_t kernelEnd,
                     const void* multibootInfo /*opaque, v1*/);

                // Allocate one 4KiB frame; returns physical address or 0 on failure
                static phys_addr_t AllocFrame();
                static void FreeFrame(phys_addr_t addr);

                static uint32_t TotalFrames();
                static uint32_t FreeFrames();

            private:
                static void markRangeUsed(phys_addr_t start, phys_addr_t end);
                static void markRangeFree(phys_addr_t start, phys_addr_t end);
        };
    }
}

#endif