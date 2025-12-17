#include "include/net/udp.hpp"
#include "include/net/ipv4.hpp"
#include "include/net/ipv4_packet.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"
#include "include/net/arp.hpp"

namespace kos {
namespace net {

// Helper to convert uint16 to network byte order
static inline uint16_t htons(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

uint32_t build_udp_packet(const uint8_t* payload, uint32_t payload_len,
                          uint32_t src_ip, uint32_t dst_ip,
                          uint16_t src_port, uint16_t dst_port,
                          uint8_t* buffer, uint32_t buffer_size) {
    if (!payload || !buffer) return 0;
    
    uint32_t udp_len = sizeof(UDPHeader) + payload_len;
    if (udp_len > buffer_size) return 0;
    
    // Build UDP header
    UDPHeader* udp = reinterpret_cast<UDPHeader*>(buffer);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons((uint16_t)udp_len);
    udp->checksum = 0;  // Optional, set to 0 for now
    
    // Copy payload
    uint8_t* payload_dst = buffer + sizeof(UDPHeader);
    for (uint32_t i = 0; i < payload_len; ++i) {
        payload_dst[i] = payload[i];
    }
    
    return udp_len;
}

bool send_udp_packet(const uint8_t* payload, uint32_t payload_len,
                     uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
    if (!payload || payload_len == 0) return false;
    
    // Build UDP packet
    uint8_t udp_buf[1024];
    
    // Get source IP from config
    kos::net::ipv4::Config cfg = kos::net::ipv4::GetConfig();
    unsigned parts[4] = {0,0,0,0};
    int pi = 0;
    unsigned val = 0;
    for (int i = 0; cfg.ip[i] != '\0'; ++i) {
        char ch = cfg.ip[i];
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
    uint32_t src_ip = (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
    
    uint32_t udp_len = build_udp_packet(payload, payload_len, src_ip, dst_ip,
                                        src_port, dst_port, udp_buf, sizeof(udp_buf));
    if (udp_len == 0) return false;
    
    // Build IPv4 packet around UDP
    uint8_t ip_buf[1024];
    uint32_t ip_len = build_ipv4_packet(udp_buf, udp_len, src_ip, dst_ip,
                                        17, /* UDP protocol */ 64, /* TTL */
                                        ip_buf, sizeof(ip_buf));
    if (ip_len == 0) return false;
    
    // Resolve MAC via ARP
    MacAddr mac{};
    bool have_mac = arp_resolve(IPv4Addr{dst_ip}, mac);
    
    // Build Ethernet frame
    uint8_t frame[1500];
    if (ip_len + sizeof(EthernetHeader) > sizeof(frame)) return false;
    
    EthernetHeader* eth = reinterpret_cast<EthernetHeader*>(frame);
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = have_mac ? mac.b[i] : 0xFF;
    }
    
    // Source MAC from NIC
    uint8_t smac[6];
    if (kos_nic_get_mac(smac)) {
        for (int i = 0; i < 6; ++i) eth->src[i] = smac[i];
    } else {
        for (int i = 0; i < 6; ++i) eth->src[i] = 0x00;
    }
    eth->ethertype = ETHERTYPE_IPV4;
    
    // Copy IP payload
    uint8_t* out = frame + sizeof(EthernetHeader);
    for (uint32_t i = 0; i < ip_len; ++i) out[i] = ip_buf[i];
    
    // Send via NIC
    uint32_t frame_len = sizeof(EthernetHeader) + ip_len;
    return kos_nic_send_frame(frame, frame_len);
}

} // namespace net
} // namespace kos
