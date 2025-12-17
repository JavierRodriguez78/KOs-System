#include "include/net/dns.hpp"
#include "include/net/udp.hpp"
#include "include/net/ipv4.hpp"
#include "include/net/ipv4_packet.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"
#include "include/net/arp.hpp"
#include <lib/string.hpp>

namespace kos {
namespace net {
namespace dns {

using namespace kos::lib;

// Helper to convert uint16 to network byte order
static inline uint16_t htons(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

// Helper to convert uint16 from network byte order
static inline uint16_t ntohs(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

// Helper to convert uint32 to network byte order
static inline uint32_t htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
}

// Helper to convert uint32 from network byte order
static inline uint32_t ntohl(uint32_t val) {
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
}

// Encode a hostname into DNS name format (length-prefixed labels)
// e.g., "www.google.com" -> [3]www[6]google[3]com[0]
static uint32_t encode_dns_name(const char* hostname, uint8_t* buffer, uint32_t buffer_size) {
    if (!hostname || !buffer || buffer_size < 2) return 0;
    
    uint32_t pos = 0;
    const char* label_start = hostname;
    const char* p = hostname;
    
    while (*p || label_start != p) {
        if (*p == '.' || *p == '\0') {
            // End of label
            uint8_t label_len = (uint8_t)(p - label_start);
            if (label_len == 0 || label_len > 63) return 0;  // Invalid label
            if (pos + 1 + label_len >= buffer_size) return 0; // Buffer too small
            
            buffer[pos++] = label_len;
            for (uint8_t i = 0; i < label_len; ++i) {
                buffer[pos++] = (uint8_t)label_start[i];
            }
            
            if (*p == '\0') break;
            label_start = p + 1;
        }
        ++p;
    }
    
    // Add null terminator
    if (pos >= buffer_size) return 0;
    buffer[pos++] = 0;
    
    return pos;
}

uint32_t build_dns_query(const char* hostname, uint16_t query_id,
                         uint8_t* buffer, uint32_t buffer_size) {
    if (!hostname || !buffer || buffer_size < sizeof(DNSHeader) + 256) return 0;
    
    uint32_t pos = 0;
    
    // DNS Header
    DNSHeader* header = reinterpret_cast<DNSHeader*>(buffer);
    header->id = htons(query_id);
    header->flags = htons(DNS_FLAG_RD);  // Recursion desired
    header->qdcount = htons(1);          // 1 question
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;
    pos += sizeof(DNSHeader);
    
    // Encode the hostname
    uint32_t name_len = encode_dns_name(hostname, buffer + pos, buffer_size - pos - 4);
    if (name_len == 0) return 0;
    pos += name_len;
    
    // Question type (A record) and class (IN)
    if (pos + 4 > buffer_size) return 0;
    buffer[pos++] = (DNS_TYPE_A >> 8) & 0xFF;
    buffer[pos++] = DNS_TYPE_A & 0xFF;
    buffer[pos++] = (DNS_CLASS_IN >> 8) & 0xFF;
    buffer[pos++] = DNS_CLASS_IN & 0xFF;
    
    return pos;
}

// Skip over a DNS name in a packet (handles compression pointers)
static uint32_t skip_dns_name(const uint8_t* packet, uint32_t packet_len, uint32_t offset) {
    uint32_t pos = offset;
    
    while (pos < packet_len) {
        uint8_t len = packet[pos];
        
        if (len == 0) {
            // End of name
            return pos + 1;
        } else if ((len & 0xC0) == 0xC0) {
            // Compression pointer (2 bytes)
            return pos + 2;
        } else {
            // Regular label
            pos += 1 + len;
        }
    }
    
    return 0;  // Invalid
}

bool parse_dns_response(const uint8_t* response, uint32_t response_len,
                        uint16_t query_id, uint32_t* ip_out) {
    if (!response || response_len < sizeof(DNSHeader) || !ip_out) return false;
    
    const DNSHeader* header = reinterpret_cast<const DNSHeader*>(response);
    
    // Check transaction ID
    if (ntohs(header->id) != query_id) return false;
    
    // Check if it's a response
    if (!(ntohs(header->flags) & DNS_FLAG_QR)) return false;
    
    // Check response code (0 = no error)
    if ((ntohs(header->flags) & DNS_FLAG_RCODE) != 0) return false;
    
    uint16_t questions = ntohs(header->qdcount);
    uint16_t answers = ntohs(header->ancount);
    
    if (answers == 0) return false;
    
    uint32_t pos = sizeof(DNSHeader);
    
    // Skip questions
    for (uint16_t i = 0; i < questions; ++i) {
        pos = skip_dns_name(response, response_len, pos);
        if (pos == 0 || pos + 4 > response_len) return false;
        pos += 4;  // Skip type and class
    }
    
    // Parse answers
    for (uint16_t i = 0; i < answers; ++i) {
        // Skip name
        pos = skip_dns_name(response, response_len, pos);
        if (pos == 0 || pos + 10 > response_len) return false;
        
        uint16_t type = (response[pos] << 8) | response[pos + 1];
        uint16_t data_len = (response[pos + 8] << 8) | response[pos + 9];
        pos += 10;
        
        if (pos + data_len > response_len) return false;
        
        // Check if it's an A record
        if (type == DNS_TYPE_A && data_len == 4) {
            // Extract IPv4 address (already in network byte order)
            *ip_out = (response[pos] << 24) | (response[pos + 1] << 16) |
                      (response[pos + 2] << 8) | response[pos + 3];
            return true;
        }
        
        pos += data_len;
    }
    
    return false;
}

// Global storage for DNS response (simple approach for now)
static uint8_t g_dns_response_buf[512];
static uint32_t g_dns_response_len = 0;
static uint16_t g_dns_response_id = 0;
static bool g_dns_response_ready = false;

// Callback for receiving UDP packets on port 53
extern "C" void dns_on_udp_packet(uint16_t src_port, uint16_t dst_port,
                                   const uint8_t* data, uint32_t len) {
    if (dst_port == 53 && len <= sizeof(g_dns_response_buf)) {
        // Store the response
        for (uint32_t i = 0; i < len; ++i) {
            g_dns_response_buf[i] = data[i];
        }
        g_dns_response_len = len;
        g_dns_response_ready = true;
    }
}

bool dns_resolve(const char* hostname, uint32_t dns_server_ip,
                 uint32_t timeout_ms, uint32_t* ip_out) {
    if (!hostname || !ip_out) return false;
    
    // Get DNS server from config if not provided
    if (dns_server_ip == 0) {
        kos::net::ipv4::Config cfg = kos::net::ipv4::GetConfig();
        // Parse DNS server IP from config
        unsigned parts[4] = {0,0,0,0};
        int pi = 0;
        unsigned val = 0;
        for (int i = 0; cfg.dns[i] != '\0'; ++i) {
            char ch = cfg.dns[i];
            if (ch >= '0' && ch <= '9') {
                val = val * 10 + (unsigned)(ch - '0');
                if (val > 255) return false;
            } else if (ch == '.') {
                if (pi >= 4) return false;
                parts[pi++] = val;
                val = 0;
            }
        }
        if (pi == 3) parts[3] = val; else return false;
        dns_server_ip = (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
    }
    
    if (dns_server_ip == 0) return false;
    
    // Build DNS query
    uint8_t query_buf[512];
    static uint16_t query_id_counter = 1;
    uint16_t query_id = query_id_counter++;
    
    uint32_t query_len = build_dns_query(hostname, query_id, query_buf, sizeof(query_buf));
    if (query_len == 0) return false;
    
    // Send UDP packet to DNS server (port 53)
    g_dns_response_ready = false;
    g_dns_response_id = query_id;
    
    bool sent = kos::net::send_udp_packet(query_buf, query_len, dns_server_ip, 53, 53);
    if (!sent) return false;
    
    // Wait for response (simple polling for now)
    // TODO: Implement proper timeout mechanism
    for (uint32_t i = 0; i < timeout_ms && !g_dns_response_ready; ++i) {
        // Simple delay loop (very rough, ~1ms per iteration)
        for (volatile int j = 0; j < 10000; ++j);
    }
    
    if (!g_dns_response_ready) return false;
    
    // Parse response
    return parse_dns_response(g_dns_response_buf, g_dns_response_len, query_id, ip_out);
}

} // namespace dns
} // namespace net
} // namespace kos

// C API wrapper for ping and other applications
extern "C" {
    
int kos_dns_resolve(const char* hostname, char* ip_str_out, int ip_str_size) {
    if (!hostname || !ip_str_out || ip_str_size < 16) return -1;
    
    uint32_t ip_be = 0;
    bool resolved = kos::net::dns::dns_resolve(hostname, 0, 5000, &ip_be);
    
    if (!resolved) return -1;
    
    // Convert IP to dotted decimal string
    unsigned char b1 = (ip_be >> 24) & 0xFF;
    unsigned char b2 = (ip_be >> 16) & 0xFF;
    unsigned char b3 = (ip_be >> 8) & 0xFF;
    unsigned char b4 = ip_be & 0xFF;
    
    int written = 0;
    // Simple snprintf alternative
    char buf[16];
    int pos = 0;
    
    // Convert each byte to string
    auto itoa = [](unsigned char val, char* b) -> int {
        if (val == 0) { b[0] = '0'; return 1; }
        int len = 0;
        char tmp[4];
        while (val > 0) {
            tmp[len++] = '0' + (val % 10);
            val /= 10;
        }
        for (int i = 0; i < len; ++i) {
            b[i] = tmp[len - 1 - i];
        }
        return len;
    };
    
    pos += itoa(b1, buf + pos);
    buf[pos++] = '.';
    pos += itoa(b2, buf + pos);
    buf[pos++] = '.';
    pos += itoa(b3, buf + pos);
    buf[pos++] = '.';
    pos += itoa(b4, buf + pos);
    buf[pos] = '\0';
    
    // Copy to output
    for (int i = 0; i <= pos && i < ip_str_size - 1; ++i) {
        ip_str_out[i] = buf[i];
    }
    ip_str_out[pos < ip_str_size ? pos : ip_str_size - 1] = '\0';
    
    return 0;
}

}
