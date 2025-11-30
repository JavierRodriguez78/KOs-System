#pragma once
#include <common/types.hpp>
namespace kos { 
    namespace arch { 
        namespace x86 { 
            namespace hardware { 
                namespace interrupts { 
                    class InterruptManager; 
                } 
            } 
        } 
    } 
}

namespace kos {
    namespace kernel {
        // Initialize core subsystems: PMM, paging, heap, scheduler, pipe manager, thread manager
        void InitCore(arch::x86::hardware::interrupts::InterruptManager* interrupts,
              kos::common::uint32_t memLowerKB, kos::common::uint32_t memUpperKB,
              const void* kernel_start, const void* kernel_end,
              const void* multiboot_structure);
    }
}
