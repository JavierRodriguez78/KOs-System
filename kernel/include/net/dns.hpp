#pragma once

#include <common/types.hpp>

namespace kos {
namespace net {
namespace dns {

using namespace kos::common;

// DNS header structure (12 bytes)
struct __attribute__((packed)) DNSHeader {
    uint16_t id;           // Transaction ID
    uint16_t flags;        // Flags
    uint16_t qdcount;      // Question count
    uint16_t ancount;      // Answer count
    uint16_t nscount;      // Authority count
    uint16_t arcount;      // Additional count
};

// DNS flag bits
constexpr uint16_t DNS_FLAG_QR      = 0x8000;  // Query/Response (0=query, 1=response)
constexpr uint16_t DNS_FLAG_OPCODE  = 0x7800;  // Opcode (0=standard query)
constexpr uint16_t DNS_FLAG_AA      = 0x0400;  // Authoritative Answer
constexpr uint16_t DNS_FLAG_TC      = 0x0200;  // Truncated
constexpr uint16_t DNS_FLAG_RD      = 0x0100;  // Recursion Desired
constexpr uint16_t DNS_FLAG_RA      = 0x0080;  // Recursion Available
constexpr uint16_t DNS_FLAG_RCODE   = 0x000F;  // Response code

// DNS query types
constexpr uint16_t DNS_TYPE_A       = 1;       // IPv4 address
constexpr uint16_t DNS_TYPE_AAAA    = 28;      // IPv6 address
constexpr uint16_t DNS_TYPE_CNAME   = 5;       // Canonical name

// DNS query class
constexpr uint16_t DNS_CLASS_IN     = 1;       // Internet

/**
 * @brief Build a DNS query packet for A record lookup
 * @param hostname The hostname to resolve (e.g., "www.google.com")
 * @param query_id Transaction ID for the query
 * @param buffer Output buffer for the DNS query packet
 * @param buffer_size Size of the output buffer
 * @return Length of the DNS query packet, or 0 on error
 */
uint32_t build_dns_query(const char* hostname, uint16_t query_id, 
                         uint8_t* buffer, uint32_t buffer_size);

/**
 * @brief Parse a DNS response and extract the first A record
 * @param response DNS response packet
 * @param response_len Length of the response packet
 * @param query_id Expected transaction ID
 * @param ip_out Output: IPv4 address in network byte order
 * @return true if an A record was found, false otherwise
 */
bool parse_dns_response(const uint8_t* response, uint32_t response_len,
                        uint16_t query_id, uint32_t* ip_out);

/**
 * @brief Resolve a hostname to an IPv4 address using DNS
 * @param hostname The hostname to resolve
 * @param dns_server_ip DNS server IP in network byte order (0 = use default from config)
 * @param timeout_ms Timeout in milliseconds
 * @param ip_out Output: Resolved IPv4 address in network byte order
 * @return true if resolution succeeded, false otherwise
 */
bool dns_resolve(const char* hostname, uint32_t dns_server_ip,
                 uint32_t timeout_ms, uint32_t* ip_out);

} // namespace dns
} // namespace net
} // namespace kos
