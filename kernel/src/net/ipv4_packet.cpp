#include "include/net/ipv4_packet.hpp"
#include "include/common/types.hpp"

namespace kos {
namespace net {

static kos::common::uint16_t ip_checksum(const void* data, kos::common::uint32_t len) {
    const kos::common::uint16_t* w = reinterpret_cast<const kos::common::uint16_t*>(data);
    kos::common::uint32_t sum = 0;
    for (kos::common::uint32_t i = 0; i < len / 2; ++i) {
        sum += w[i];
    }
    if (len & 1) {
        sum += static_cast<const kos::common::uint8_t*>(data)[len - 1] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<kos::common::uint16_t>(~sum);
}

kos::common::uint32_t build_ipv4_packet(
    const kos::common::uint8_t* payload,
    kos::common::uint32_t payload_len,
    kos::common::uint32_t src_ip_be,
    kos::common::uint32_t dst_ip_be,
    kos::common::uint8_t protocol,
    kos::common::uint8_t ttl,
    kos::common::uint8_t* out_buf,
    kos::common::uint32_t out_buf_len) {

    const kos::common::uint32_t ihl = 20; // bytes
    const kos::common::uint32_t total = ihl + payload_len;
    if (out_buf_len < total) return 0;

    IPv4Header* hdr = reinterpret_cast<IPv4Header*>(out_buf);
    hdr->ver_ihl = (4u << 4) | (ihl / 4);
    hdr->tos = 0;
    hdr->total_length = static_cast<kos::common::uint16_t>(total);
    hdr->identification = 0;
    hdr->flags_frag = 0;
    hdr->ttl = ttl;
    hdr->protocol = protocol;
    hdr->header_checksum = 0;
    hdr->src = src_ip_be;
    hdr->dst = dst_ip_be;

    hdr->header_checksum = ip_checksum(hdr, ihl);

    kos::common::uint8_t* p = out_buf + ihl;
    for (kos::common::uint32_t i = 0; i < payload_len; ++i) p[i] = payload[i];

    return total;
}

}
}
