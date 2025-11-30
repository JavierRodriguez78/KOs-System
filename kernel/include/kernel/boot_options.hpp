#pragma once
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    
    namespace kernel {

        /*
        @brief Display mode for the kernel.
        */
        enum class DisplayMode : uint8_t {
            Text = 0,
            Graphics = 1
        };

        class BootOptions {
            public:
                /*
                @brief Parse boot options from Multiboot information structure.
                @param mb_info Pointer to Multiboot information structure.
                @param magic Multiboot magic number.
                @return Parsed BootOptions instance.
                */
                static BootOptions ParseFromMultiboot(const void* mb_info, kos::common::uint32_t magic);
            
                /*
                @brief Get the mouse poll mode.
                @return Mouse poll mode as uint8_t.
                */
                uint8_t MousePollMode() const { return mousePollMode; }
    
                /*
                @brief Check if debug mode is enabled.
                @return True if debug mode is enabled, false otherwise.
                */
                bool DebugEnabled() const { return debugEnabled; }
    
                /*
                @brief Check if the system should reboot on panic.
                @return True if the system should reboot on panic, false otherwise.
                */
                bool RebootOnPanic() const { return rebootOnPanic; }
    
                /*
                @brief Get the display mode.
                @return Display mode.
                */
                DisplayMode Mode() const { return mode; }

            private:
                
                /*
                @brief Private constructor to enforce usage of ParseFromMultiboot.
                */
                BootOptions();
                
                /*
                @brief Mouse poll mode.
                */
                uint8_t mousePollMode;
              
                /*
                @brief Debug mode enabled flag.
                */
                bool debugEnabled;
              
                /*
                @brief Reboot on panic flag.
                */
                bool rebootOnPanic;
              
                /*
                @brief Display mode.
                */
                DisplayMode mode;
        };
    }
}