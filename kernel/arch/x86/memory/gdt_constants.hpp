#pragma once

#include <common/types.hpp>
using namespace kos::common;

namespace kos {
    namespace arch {
        namespace x86 {
            namespace memory{
        
                // GDT constants
                constexpr uint8_t  GDT_NULL_SEGMENT       = 0;
                constexpr uint8_t  GDT_CODE_SEGMENT       = 1;
                constexpr uint8_t  GDT_DATA_SEGMENT       = 2;
                constexpr uint8_t  GDT_USER_CODE_SEGMENT  = 3;
                constexpr uint8_t  GDT_USER_DATA_SEGMENT  = 4;
                constexpr uint8_t  GDT_TSS_SEGMENT        = 5;
                constexpr uint8_t  GDT_ENTRIES            = 6;
                constexpr uint32_t GDT_SEGMENT_LIMIT      = 64 * 1024 * 1024; // 64 MB
                constexpr uint8_t  GDT_CODE_SEGMENT_TYPE  = 0x9A; // Code segment: execute/read, ring 0
                constexpr uint8_t  GDT_DATA_SEGMENT_TYPE  = 0x92; // Data segment: read/write, ring 0
                
                // 16-bit segment constants
                constexpr uint32_t GDT_LIMIT_16BIT_MAX    = 0x10000; // 64 KiB
                constexpr uint8_t  GDT_FLAG_16BIT         = 0x40;    // D/B = 1, G = 0
                constexpr uint8_t  GDT_FLAG_32BIT         = 0xC0;    // D/B = 1, G = 1 (4 KiB pages)

                // Paging constants
                constexpr uint32_t GDT_PAGE_MASK          = 0xFFF; // 4 KiB
                constexpr uint8_t  GDT_PAGE_SHIFT         = 12;     // 4 KiB

            } // namespace memory
        } // namespace x86
    } // namespace arch
}