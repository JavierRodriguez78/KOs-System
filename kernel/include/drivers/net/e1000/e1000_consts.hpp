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

            }
        }
    }
}
