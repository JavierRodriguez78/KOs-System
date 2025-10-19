#pragma once

#include <common/types.hpp>

using namespace kos::common;


namespace kos {

    namespace arch {
    
        namespace x86 {
        
            namespace hardware {
            
                namespace pci {
                
                    // --------------------------------------------------
                    // PCI base I/O ports
                    // --------------------------------------------------
                    constexpr uint16_t PCI_DATA_PORT = 0xCFC;
                    constexpr uint16_t PCI_COMMAND_PORT = 0xCF8;

                    // --------------------------------------------------
                    // Configuration bits
                    // --------------------------------------------------
                    constexpr uint32_t PCI_ENABLE_BIT = 0x1 << 31;

                    // --------------------------------------------------
                    // Field bit shifts
                    // --------------------------------------------------
                    constexpr uint8_t PCI_BUS_SHIFT = 16;
                    constexpr uint8_t PCI_DEVICE_SHIFT = 11;
                    constexpr uint8_t PCI_FUNCTION_SHIFT = 8;

                    // --------------------------------------------------
                    // Field masks
                    // --------------------------------------------------
                    constexpr uint8_t PCI_BUS_MASK = 0xFF;
                    constexpr uint8_t PCI_DEVICE_MASK = 0x1F;
                    constexpr uint8_t PCI_FUNCTION_MASK = 0x07;
                    constexpr uint8_t PCI_REGISTER_MASK = 0xFC;

                    // --------------------------------------------------
                    // Standard PCI configuration register offsets
                    // --------------------------------------------------
                    constexpr uint8_t PCI_VENDOR_ID_OFFSET = 0x00;
                    constexpr uint8_t PCI_DEVICE_ID_OFFSET = 0x02;
                    constexpr uint8_t PCI_REVISION_ID_OFFSET = 0x08;
                    constexpr uint8_t PCI_PROG_IF_OFFSET = 0x09;
                    constexpr uint8_t PCI_SUBCLASS_OFFSET = 0x0A;
                    constexpr uint8_t PCI_CLASS_OFFSET = 0x0B;
                    constexpr uint8_t PCI_HEADER_TYPE_OFFSET = 0x0E;
                    constexpr uint8_t PCI_INTERRUPT_LINE_OFFSET = 0x3C;

                    // --------------------------------------------------
                    // Bit positions within specific fields
                    // --------------------------------------------------
                    constexpr uint8_t PCI_MULTI_FUNCTION_BIT = 7;

                    // --------------------------------------------------
                    // Special vendor ID values
                    // --------------------------------------------------
                    constexpr uint16_t PCI_INVALID_VENDOR_1 = 0x0000;
                    constexpr uint16_t PCI_INVALID_VENDOR_2 = 0xFFFF;

                    // --------------------------------------------------
                    // PCI enumeration limits
                    // --------------------------------------------------
                    constexpr int32_t PCI_MAX_BUSES = 8;
                    constexpr int32_t PCI_MAX_DEVICES = 32;
                    constexpr int32_t PCI_MAX_FUNCTIONS = 8;
                }
            }
         }
    }
}