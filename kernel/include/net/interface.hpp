#pragma once

#include <common/types.hpp>

namespace kos {
    namespace net {
        namespace iface {

            struct Interface {
                char name[8];
                char mac[18]; // "aa:bb:cc:dd:ee:ff"
                uint32_t mtu;
                bool up;
                bool running;
                // Simple RX/TX counters
                uint64_t rx_packets;
                uint64_t rx_bytes;
                uint64_t tx_packets;
                uint64_t tx_bytes;
            };

            void SetInterface(const Interface& ifc);
            const Interface& GetInterface();

        } // namespace iface
    } // namespace net
} // namespace kos
