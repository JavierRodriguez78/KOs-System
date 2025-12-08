#pragma once

#include "include/common/types.hpp"
using namespace kos::common;

namespace kos {
    namespace net {

        // Minimal IPv4 header (without options)
        struct IPv4Header {
            uint8_t  ver_ihl;      // version(4) + IHL(4)
            uint8_t  tos;
            uint16_t total_length; // network byte order
            uint16_t identification;
            uint16_t flags_frag;   // flags(3) + fragment offset(13)
            uint8_t  ttl;
            uint8_t  protocol;
            uint16_t header_checksum;
            uint32_t src;
            uint32_t dst;
        };

        // Build a minimal IPv4 packet to wrap `payload`.
        // Returns number of bytes written to out_buf, or 0 on error.
        uint32_t build_ipv4_packet(
            const uint8_t* payload,
            uint32_t payload_len,
            uint32_t src_ip_be,
            uint32_t dst_ip_be,
            uint8_t protocol,
            uint8_t ttl,
            uint8_t* out_buf,
            uint32_t out_buf_len);

    }
}
