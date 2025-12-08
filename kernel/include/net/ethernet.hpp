#pragma once

#include "include/common/types.hpp"

namespace kos { 
    namespace net {

        /*
        @brief Minimal Ethernet header structure.
        */
        struct EthernetHeader {
            uint8_t dst[6]; // Destination MAC address
            uint8_t src[6]; // Source MAC address
            uint16_t ethertype; // big-endian value
        };

        static const uint16_t ETHERTYPE_IPV4 = 0x0800; //   IPv4 Ethertype
        static const uint16_t ETHERTYPE_ARP  = 0x0806; //   ARP Ethertype
    } 
}
