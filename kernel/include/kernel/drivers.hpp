#pragma once
#include <common/types.hpp>
namespace kos { 
    namespace arch { 
        namespace x86 { 
            namespace hardware { 
                namespace interrupts { class InterruptManager; 
                } 
            } 
        } 
    } 
}

namespace kos { 
    namespace kernel {
        // Initialize and activate hardware drivers. Requires an active InterruptManager.
        void InitDrivers(arch::x86::hardware::interrupts::InterruptManager* interrupts);
    }
}
