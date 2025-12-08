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

                // PCI BARs
                static constexpr uint8_t  PCI_BAR0_OFFSET   = 0x10;      // Config space offset for BAR0
                static constexpr uint32_t PCI_BAR_ADDR_MASK = 0xFFFFFFF0u; // Mask to extract base address (I/O/MMIO)

                // MMIO register offsets (subset)
                static constexpr uint32_t REG_RAL0   = 0x5400; // Receive Address Low 0
                static constexpr uint32_t REG_RAH0   = 0x5404; // Receive Address High 0
                static constexpr uint32_t REG_TDBAL  = 0x3800; // Transmit Descriptor Base Address Low
                static constexpr uint32_t REG_TDBAH  = 0x3804; // Transmit Descriptor Base Address High
                static constexpr uint32_t REG_TDLEN  = 0x3808; // Transmit Descriptor Length
                static constexpr uint32_t REG_TCTL   = 0x0400; // Transmit Control
                static constexpr uint32_t REG_TIPG   = 0x0410; // Transmit Inter-Packet Gap

                // TCTL bits (subset)
                static constexpr uint32_t TCTL_EN    = 0x00000002; // Transmit Enable
                static constexpr uint32_t TCTL_PSP   = 0x00000008; // Pad Short Packets
                static constexpr uint32_t TCTL_CT_SHIFT = 4;        // Collision Threshold
                static constexpr uint32_t TCTL_COLD_SHIFT = 12;     // Collision Distance

            }
        }
    }
}
