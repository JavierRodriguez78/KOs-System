#pragma once
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace kernel {
        /*
        @brief Class to parse and manage Multiboot kernel information.
        */
        class MultibootKernel {
            public:
                /*
                @brief Constructor for MultibootKernel.
                @param mb_info Pointer to Multiboot information structure.
                @param magic Multiboot magic number.        
                */
                MultibootKernel(const void* mb_info, uint32_t magic);
                
                /*
                @brief Initialize parsing and any multiboot-driven subsystems (framebuffer).
                */
                void Init();

                /*
                @brief Get the mouse poll mode.
                @return Mouse poll mode as uint8_t. 
                */
                uint8_t MousePollMode() const { return mousePollMode; }
                
                /*
                @brief Get the lower memory size in KB.     
                @return Lower memory size in KB.
                */
                uint32_t MemLowerKB() const { return memLowerKB; }
                
                /*
                @brief Get the upper memory size in KB.
                @return Upper memory size in KB.
                */
                uint32_t MemUpperKB() const { return memUpperKB; }

            private:
                /*
                @brief Pointer to the Multiboot information structure.
                */
                const void* mb_info;
                
                /*
                @brief Multiboot magic number.
                */
                uint32_t magic;
                
                /*
                @brief Mouse poll mode.
                */
                uint8_t mousePollMode;
                
                /*
                @brief Lower memory size in KB.
                */
                uint32_t memLowerKB;
                
                /*
                @brief Upper memory size in KB.
                */
                uint32_t memUpperKB;
        };
    } 
}
