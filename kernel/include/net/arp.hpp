#pragma once

#include "include/common/types.hpp"

using namespace kos::common;

namespace kos {
    namespace net {

        /*
        @brief Represents a MAC address and an IPv4 address.
        */
        struct MacAddr {
            uint8_t b[6]; // 6 bytes MAC address
        };

        /*
        @brief Represents an IPv4 address.
        */
        struct IPv4Addr {
            uint32_t addr; // network byte order
        };
               
        /*
        @brief Attempts to resolve the MAC address for an IPv4 destination using ARP.
        @param ip The IPv4 address to resolve.
        @param mac_out Output parameter to receive the resolved MAC address.
        @return True if the MAC address was successfully resolved; false otherwise.
        */
        bool arp_resolve(const IPv4Addr& ip, MacAddr& mac_out);

        /*
        @brief Ingests a raw Ethernet frame that may contain an ARP packet.
        @param frame Pointer to the raw Ethernet frame data.
        @param len Length of the frame data in bytes.
        @return True if the frame was an ARP packet and was processed; false otherwise.
        */
        bool arp_ingest(const uint8_t* frame, uint32_t len);

    }
}
