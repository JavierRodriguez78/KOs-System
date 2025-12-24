#include "include/net/rx_dispatch.hpp"
#include "include/net/nic.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/arp.hpp"
#include "include/net/udp.hpp"

// Forward declarations
extern "C" void dns_on_udp_packet(uint16_t src_port, uint16_t dst_port,
                                   const uint8_t* data, uint32_t len);

namespace kos { namespace net {

// Simple IP header for parsing
struct IPv4Header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
};

static inline uint16_t ntohs(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

static void rx_cb(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    if (len < sizeof(EthernetHeader)) return;
    const EthernetHeader* eth = reinterpret_cast<const EthernetHeader*>(frame);
    const kos::common::uint8_t* payload = frame + sizeof(EthernetHeader);
    kos::common::uint32_t plen = len - sizeof(EthernetHeader);
    if (eth->ethertype == ETHERTYPE_ARP) {
        arp_ingest(frame, len);
        return;
    }
    if (eth->ethertype == ETHERTYPE_IPV4) {
        // Parse IP header
        if (plen < sizeof(IPv4Header)) return;
        const IPv4Header* ip = reinterpret_cast<const IPv4Header*>(payload);
        uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
        if (plen < ihl) return;
        
        const uint8_t* ip_payload = payload + ihl;
        uint32_t ip_payload_len = plen - ihl;
        
        // Check protocol
        if (ip->protocol == 17) { // UDP
            if (ip_payload_len < sizeof(UDPHeader)) return;
            const UDPHeader* udp = reinterpret_cast<const UDPHeader*>(ip_payload);
            uint16_t src_port = ntohs(udp->src_port);
            uint16_t dst_port = ntohs(udp->dst_port);
            const uint8_t* udp_data = ip_payload + sizeof(UDPHeader);
            uint32_t udp_data_len = ip_payload_len - sizeof(UDPHeader);
            
            // Forward DNS responses (from port 53)
            if (src_port == 53) {
                dns_on_udp_packet(src_port, dst_port, udp_data, udp_data_len);
            }
            return;
        }
        
        if (ip->protocol == 1) { // ICMP
            // Forward to raw ICMP handler if present
            extern void rawicmp_on_ipv4_packet(const kos::common::uint8_t* data, kos::common::uint32_t len);
            rawicmp_on_ipv4_packet(payload, plen);
            return;
        }
    }
}

void rx_dispatch_init() {
    kos_nic_register_rx(rx_cb);
}

} }
