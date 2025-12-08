#include "include/net/arp.hpp"
#include "include/net/arp_cache.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"
#include "include/net/ipv4.hpp"

namespace kos {
namespace net {

bool arp_resolve(const IPv4Addr& ip, MacAddr& mac_out) {
    if (arp_cache_lookup(ip, mac_out)) return true;
    arp_send_request(ip);
    return false;
}

// Minimal ARP packet (Ethernet/IPv4)
struct ArpPacket {
    kos::common::uint16_t hw_type;   // Ethernet = 1
    kos::common::uint16_t proto_type; // IPv4 = 0x0800
    kos::common::uint8_t  hw_size;   // 6
    kos::common::uint8_t  proto_size;// 4
    kos::common::uint16_t op;        // 1=request, 2=reply
    kos::common::uint8_t  sha[6];    // sender MAC
    kos::common::uint8_t  sip[4];    // sender IP
    kos::common::uint8_t  tha[6];    // target MAC
    kos::common::uint8_t  tip[4];    // target IP
};

static void ip_be_to_bytes(kos::common::uint32_t be, kos::common::uint8_t out[4]) {
    out[0] = (be >> 24) & 0xFF;
    out[1] = (be >> 16) & 0xFF;
    out[2] = (be >> 8) & 0xFF;
    out[3] = (be) & 0xFF;
}

static kos::common::uint32_t parse_ipv4_be_str(const char* s) {
    unsigned parts[4] = {0,0,0,0};
    int pi = 0; unsigned val = 0;
    for (int i = 0; s && s[i] != '\0'; ++i) {
        char ch = s[i];
        if (ch >= '0' && ch <= '9') {
            val = val * 10 + (unsigned)(ch - '0');
            if (val > 255) return 0;
        } else if (ch == '.') {
            if (pi >= 4) return 0;
            parts[pi++] = val; val = 0;
        } else { return 0; }
    }
    if (pi != 3) return 0;
    parts[3] = val;
    return (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
}

void arp_send_request(const IPv4Addr& ip) {
    // Build Ethernet broadcast + ARP request using NIC MAC and current IP if available
    kos::common::uint8_t frame[sizeof(kos::net::EthernetHeader) + sizeof(ArpPacket)];
    kos::net::EthernetHeader* eth = reinterpret_cast<kos::net::EthernetHeader*>(frame);
    for (int i = 0; i < 6; ++i) { eth->dst[i] = 0xFF; }
    kos::common::uint8_t mac[6];
    if (kos_nic_get_mac(mac)) { for (int i = 0; i < 6; ++i) eth->src[i] = mac[i]; }
    else { for (int i = 0; i < 6; ++i) eth->src[i] = 0x00; }
    eth->ethertype = kos::net::ETHERTYPE_ARP;
    ArpPacket* arp = reinterpret_cast<ArpPacket*>(frame + sizeof(kos::net::EthernetHeader));
    arp->hw_type = 1;
    arp->proto_type = kos::net::ETHERTYPE_IPV4;
    arp->hw_size = 6;
    arp->proto_size = 4;
    arp->op = 1; // request
    // Sender MAC/IP
    if (kos_nic_get_mac(mac)) { for (int i = 0; i < 6; ++i) arp->sha[i] = mac[i]; }
    else { for (int i = 0; i < 6; ++i) arp->sha[i] = 0x00; }
    for (int i = 0; i < 6; ++i) arp->tha[i] = 0x00;
    kos::common::uint8_t ipb[4]; ip_be_to_bytes(ip.addr, ipb);
    // sip=0.0.0.0, tip=target
    kos::net::ipv4::Config cfg = kos::net::ipv4::GetConfig();
    kos::common::uint32_t sip_be = parse_ipv4_be_str(cfg.ip);
    kos::common::uint8_t sipb[4]; ip_be_to_bytes(sip_be, sipb);
    for (int i = 0; i < 4; ++i) arp->sip[i] = sipb[i];
    for (int i = 0; i < 4; ++i) arp->tip[i] = ipb[i];
    (void)kos_nic_send_frame(frame, sizeof(frame));
}

bool arp_ingest(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    if (len < sizeof(kos::net::EthernetHeader) + sizeof(ArpPacket)) return false;
    const kos::net::EthernetHeader* eth = reinterpret_cast<const kos::net::EthernetHeader*>(frame);
    if (eth->ethertype != kos::net::ETHERTYPE_ARP) return false;
    const ArpPacket* arp = reinterpret_cast<const ArpPacket*>(frame + sizeof(kos::net::EthernetHeader));
    if (arp->op != 2) return false; // reply only
    IPv4Addr sip{}; sip.addr = (arp->sip[0]<<24)|(arp->sip[1]<<16)|(arp->sip[2]<<8)|arp->sip[3];
    MacAddr mac{}; for (int i = 0; i < 6; ++i) mac.b[i] = arp->sha[i];
    arp_cache_update(sip, mac);
    return true;
}

// removed old stub

}
}
