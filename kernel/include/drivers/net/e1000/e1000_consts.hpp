#pragma once

#include <lib/libc/stdint.h>

// Intel E1000 (8254x) common constants extracted from driver usage

namespace kos {
    namespace drivers {
        namespace net {
            namespace e1000 {

                // PCI identification
                static constexpr uint16_t PCI_VENDOR_INTEL = 0x8086;
                static constexpr uint16_t PCI_DEVICE_82540EM = 0x100E; // QEMU/VirtualBox default
                static constexpr uint16_t PCI_DEVICE_82545EM = 0x100F;
                static constexpr uint16_t PCI_DEVICE_82543GC = 0x1004;

                // PCI BARs
                static constexpr uint8_t  PCI_BAR0_OFFSET   = 0x10;      // Config space offset for BAR0
                static constexpr uint32_t PCI_BAR_ADDR_MASK = 0xFFFFFFF0u; // Mask to extract base address (I/O/MMIO)

                // MMIO register offsets (subset)
                static constexpr uint32_t REG_CTRL   = 0x0000; // Device Control
                static constexpr uint32_t REG_STATUS = 0x0008; // Device Status
                static constexpr uint32_t REG_EECD   = 0x0010; // EEPROM/Flash Control
                static constexpr uint32_t REG_RAL0   = 0x5400; // Receive Address Low 0
                static constexpr uint32_t REG_RAH0   = 0x5404; // Receive Address High 0
                static constexpr uint32_t REG_RDBAL  = 0x2800; // Receive Descriptor Base Address Low
                static constexpr uint32_t REG_RDBAH  = 0x2804; // Receive Descriptor Base Address High
                static constexpr uint32_t REG_RDLEN  = 0x2808; // Receive Descriptor Length
                static constexpr uint32_t REG_RDH    = 0x2810; // Receive Descriptor Head
                static constexpr uint32_t REG_RDT    = 0x2818; // Receive Descriptor Tail
                static constexpr uint32_t REG_RCTL   = 0x0100; // Receive Control
                static constexpr uint32_t REG_TDBAL  = 0x3800; // Transmit Descriptor Base Address Low
                static constexpr uint32_t REG_TDBAH  = 0x3804; // Transmit Descriptor Base Address High
                static constexpr uint32_t REG_TDLEN  = 0x3808; // Transmit Descriptor Length
                static constexpr uint32_t REG_TDH    = 0x3810; // Transmit Descriptor Head
                static constexpr uint32_t REG_TDT    = 0x3818; // Transmit Descriptor Tail
                static constexpr uint32_t REG_TCTL   = 0x0400; // Transmit Control
                static constexpr uint32_t REG_TIPG   = 0x0410; // Transmit Inter-Packet Gap
                static constexpr uint32_t REG_ICR    = 0x00C0; // Interrupt Cause Read (read-to-clear)
                static constexpr uint32_t REG_IMS    = 0x00D0; // Interrupt Mask Set
                static constexpr uint32_t REG_IMC    = 0x00D8; // Interrupt Mask Clear

                // CTRL bits
                static constexpr uint32_t CTRL_RST   = 0x04000000; // Device Reset
                static constexpr uint32_t CTRL_SLU   = 0x00000040; // Set Link Up
                
                // RCTL bits
                static constexpr uint32_t RCTL_EN    = 0x00000002; // Receiver Enable
                static constexpr uint32_t RCTL_UPE   = 0x00000008; // Unicast Promiscuous Enable
                static constexpr uint32_t RCTL_MPE   = 0x00000010; // Multicast Promiscuous Enable
                static constexpr uint32_t RCTL_BAM   = 0x00008000; // Broadcast Accept Mode

                // TCTL bits (subset)
                static constexpr uint32_t TCTL_EN    = 0x00000002; // Transmit Enable
                static constexpr uint32_t TCTL_PSP   = 0x00000008; // Pad Short Packets
                static constexpr uint32_t TCTL_CT_SHIFT = 4;        // Collision Threshold
                static constexpr uint32_t TCTL_COLD_SHIFT = 12;     // Collision Distance
                
                // Descriptor ring sizes
                static constexpr uint32_t TX_DESC_COUNT = 8;       // Number of TX descriptors
                static constexpr uint32_t RX_DESC_COUNT = 8;       // Number of RX descriptors
                static constexpr uint32_t TX_BUFFER_SIZE = 2048;   // TX buffer size per descriptor
                static constexpr uint32_t RX_BUFFER_SIZE = 2048;   // RX buffer size per descriptor

            }
        }
    }
}
