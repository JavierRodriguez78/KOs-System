#pragma once

#include <common/types.hpp>

namespace kos {
namespace net {

using namespace kos::common;

// UDP header structure
struct __attribute__((packed)) UDPHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;     // Length of header + data
    uint16_t checksum;   // Optional (can be 0)
};

/**
 * @brief Build a UDP packet
 * @param payload The UDP payload data
 * @param payload_len Length of the payload
 * @param src_ip Source IPv4 address (network byte order)
 * @param dst_ip Destination IPv4 address (network byte order)
 * @param src_port Source UDP port
 * @param dst_port Destination UDP port
 * @param buffer Output buffer for the complete IPv4+UDP packet
 * @param buffer_size Size of the output buffer
 * @return Length of the IPv4+UDP packet, or 0 on error
 */
uint32_t build_udp_packet(const uint8_t* payload, uint32_t payload_len,
                          uint32_t src_ip, uint32_t dst_ip,
                          uint16_t src_port, uint16_t dst_port,
                          uint8_t* buffer, uint32_t buffer_size);

/**
 * @brief Send a UDP packet via the NIC
 * @param payload The UDP payload data
 * @param payload_len Length of the payload
 * @param dst_ip Destination IPv4 address (network byte order)
 * @param src_port Source UDP port
 * @param dst_port Destination UDP port
 * @return true if sent successfully, false otherwise
 */
bool send_udp_packet(const uint8_t* payload, uint32_t payload_len,
                     uint32_t dst_ip, uint16_t src_port, uint16_t dst_port);

} // namespace net
} // namespace kos
