#pragma once

#include <lib/libc/stdint.h>

// Realtek RTL8169/8168/810x PCI IDs and BAR constants used by the rtl8169 driver scaffold
namespace kos {
    namespace drivers {
        namespace net {
            namespace rtl8169 {

                // PCI Vendor
                static constexpr uint16_t PCI_VENDOR_REALTEK   = 0x10EC;

                // Common device IDs for the 8169-family (PCIe/PCI variants)
                static constexpr uint16_t PCI_DEVICE_RTL8169   = 0x8169; // RTL8169 (PCI)
                static constexpr uint16_t PCI_DEVICE_RTL8168   = 0x8168; // RTL8168/8111 (PCIe)
                static constexpr uint16_t PCI_DEVICE_RTL8101   = 0x8136; // RTL8101E/8102E (Fast Ethernet)

                // PCI BAR0 offset and mask for I/O regions
                static constexpr uint8_t  PCI_BAR0_OFFSET      = 0x10;        // Config space BAR0
                static constexpr uint32_t PCI_BAR_IO_MASK      = 0xFFFFFFFCu; // I/O base mask

            }
        }
    }
}
