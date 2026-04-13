#pragma once
#ifndef __KOS__MEMORY__HEAP_H
#define __KOS__MEMORY__HEAP_H

#include <common/types.hpp>
#include <memory/memory.hpp>

namespace kos { 
    namespace memory {

        // Kernel heap allocator: page-backed allocator with split/coalesce support.
        class Heap {
            public:
                // Initialize heap at the given base virtual address and map 'initialPages' pages
                static void Init(virt_addr_t base, uint32_t initialPages);

                // Allocate 'size' bytes; alignment at least 'align' (power of two). Returns 0 on failure.
                static void* Alloc(uint32_t size, uint32_t align = 8);

                // Free previously allocated memory; ignored for invalid/null pointers.
                static void Free(void* ptr);

                // Currently live allocated bytes (payload only).
                static uint32_t Used();

                // Current mapped heap extent (debugging / compatibility)
                static virt_addr_t Brk();
                static virt_addr_t End();

            private:
                static bool ensure(virt_addr_t upto);
        };
    }
}

#endif