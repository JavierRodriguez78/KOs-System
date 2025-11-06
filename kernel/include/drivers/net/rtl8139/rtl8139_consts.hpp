#pragma once

#include <lib/libc/stdint.h>

// Realtek RTL8139 PCI and BAR constants extracted from driver usage
// Central place for IDs and BAR decoding masks/offsets
namespace kos {
    namespace drivers {
        namespace net {
            namespace rtl8139 { // avoid clash with class namespace

                // PCI identification
                static constexpr uint16_t PCI_VENDOR_REALTEK   = 0x10EC;
                static constexpr uint16_t PCI_DEVICE_RTL8139   = 0x8139;

                // PCI BARs
                static constexpr uint8_t  PCI_BAR0_OFFSET      = 0x10;        // Config space BAR0
                static constexpr uint32_t PCI_BAR_IO_MASK      = 0xFFFFFFFCu; // I/O space base mask

            }
        }
    }
}
