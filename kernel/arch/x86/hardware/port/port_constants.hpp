#pragma once

#include <common/types.hpp>

using namespace kos::common;

namespace kos {
    namespace arch {
    
        namespace x86 {
        
            namespace hardware {
            
                namespace port {
                
                    // Common port constants
                    static constexpr uint16_t PORT_KEYBOARD_DATA = 0x60;
                    static constexpr uint16_t PORT_KEYBOARD_STATUS = 0x64;
                    static constexpr uint16_t PORT_PIC1_COMMAND = 0x20;
                    static constexpr uint16_t PORT_PIC1_DATA = 0x21;
                    static constexpr uint16_t PORT_PIC2_COMMAND = 0xA0;
                    static constexpr uint16_t PORT_PIC2_DATA = 0xA1;
                    

                    constexpr uint8_t PORT_SIZE_8BIT  = 8;
                    constexpr uint8_t PORT_SIZE_16BIT = 16;
                    constexpr uint8_t PORT_SIZE_32BIT = 32;

                } // namespace port

            } // namespace hardware
            
        } // namespace x86
        
    } // namespace arch
}