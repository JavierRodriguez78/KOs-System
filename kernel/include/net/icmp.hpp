#pragma once

#include <common/types.hpp>
using namespace kos::common;

namespace kos { 
    namespace net { 
        namespace icmp {

            // ICMP type/code for Echo
            static const uint8_t ICMP_ECHO_REQUEST = 8;
            static const uint8_t ICMP_ECHO_REPLY   = 0;

            struct EchoHeader {
                uint8_t type;
                uint8_t code;
                uint16_t checksum;
                uint16_t ident;
                uint16_t seq;
            };

            // Internet checksum (RFC 1071) over a buffer
            uint16_t checksum(const void* data, uint32_t len);  
            
            // Build an ICMP Echo Request packet into `out` with payload `payload` of length `plen`.
            // Returns total packet length (header + payload), or 0 on error.
            uint32_t build_echo_request(uint16_t ident,
                                         uint16_t seq,
                                         const void* payload,
                                         uint16_t plen,
                                         void* out,
                                         uint32_t outMax);
        }
    }
}
