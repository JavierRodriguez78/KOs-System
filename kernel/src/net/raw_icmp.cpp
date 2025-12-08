#include "include/net/raw_icmp.hpp"
#include "include/net/icmp.hpp"
#include "include/net/ipv4_packet.hpp"
#include "include/net/arp.hpp"
#include "include/net/ipv4.hpp"
#include "include/common/types.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"

namespace kos {
namespace net {

RawIcmpHandle rawicmp_open() { return {0}; }
void rawicmp_close(RawIcmpHandle /*h*/) {}

bool rawicmp_send_echo(
    RawIcmpHandle /*h*/,
    kos::common::uint32_t dst_ip_be,
    kos::common::uint16_t id,
    kos::common::uint16_t seq,
    const kos::common::uint8_t* payload,
    kos::common::uint32_t payload_len,
    kos::common::uint32_t /*timeout_ms*/) {
    // Build ICMP Echo Request payload
    kos::common::uint8_t icmp_buf[64];
    kos::common::uint32_t icmp_len = kos::net::icmp::build_echo_request(id, seq, payload, payload_len, icmp_buf, sizeof(icmp_buf));
    if (icmp_len == 0) return false;

    // Build IPv4 packet around ICMP
    kos::common::uint8_t ip_buf[96];
    kos::net::ipv4::Config cfg = kos::net::ipv4::GetConfig();
    // Parse dotted decimal to BE uint32
    unsigned parts[4] = {0,0,0,0}; int pi = 0; unsigned val = 0;
    for (int i = 0; cfg.ip[i] != '\0'; ++i) {
        char ch = cfg.ip[i];
        if (ch >= '0' && ch <= '9') { val = val*10 + (unsigned)(ch-'0'); if (val>255) { val=0; break; } }
        else if (ch == '.') { parts[pi++] = val; val = 0; if (pi>3) break; }
        else { break; }
    }
    if (pi == 3) parts[3] = val; else { parts[0]=0; parts[1]=0; parts[2]=0; parts[3]=0; }
    kos::common::uint32_t src_ip_be = (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
    kos::common::uint32_t ip_len = build_ipv4_packet(
        icmp_buf, icmp_len,
        /*src_ip_be*/src_ip_be,
        dst_ip_be,
        /*protocol*/1, /* ICMP */
        /*ttl*/64,
        ip_buf, sizeof(ip_buf));
    if (ip_len == 0) return false;

    // Resolve MAC via ARP (fallback to broadcast)
    kos::net::MacAddr mac{};
    bool have_mac = arp_resolve(kos::net::IPv4Addr{dst_ip_be}, mac);

    // Build Ethernet frame
    kos::common::uint8_t frame[128];
    if (ip_len + sizeof(kos::net::EthernetHeader) > sizeof(frame)) return false;
    kos::net::EthernetHeader* eth = reinterpret_cast<kos::net::EthernetHeader*>(frame);
    // Destination MAC
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = have_mac ? mac.b[i] : 0xFF;
    }
    // Source MAC from NIC
    kos::common::uint8_t smac[6];
    if (kos_nic_get_mac(smac)) { for (int i = 0; i < 6; ++i) eth->src[i] = smac[i]; }
    else { for (int i = 0; i < 6; ++i) eth->src[i] = 0x00; }
    eth->ethertype = kos::net::ETHERTYPE_IPV4;

    // Copy IP payload after header
    kos::common::uint8_t* out = frame + sizeof(kos::net::EthernetHeader);
    for (kos::common::uint32_t i = 0; i < ip_len; ++i) out[i] = ip_buf[i];

    // Send via NIC
    kos::common::uint32_t frame_len = sizeof(kos::net::EthernetHeader) + ip_len;
    return kos_nic_send_frame(frame, frame_len);
}

// Simple storage for last received echo reply
struct EchoReply {
    kos::common::uint16_t id;
    kos::common::uint16_t seq;
    kos::common::uint32_t len;
    kos::common::uint8_t  buf[128];
};
static EchoReply g_queue[4];
static int g_q_head = 0;
static int g_q_tail = 0;
static void queue_reply(kos::common::uint16_t id, kos::common::uint16_t seq, const kos::common::uint8_t* data, kos::common::uint32_t len) {
    EchoReply* e = &g_queue[g_q_tail];
    e->id = id; e->seq = seq; e->len = (len > 128 ? 128 : len);
    for (kos::common::uint32_t i = 0; i < e->len; ++i) e->buf[i] = data[i];
    g_q_tail = (g_q_tail + 1) & 3;
    if (g_q_tail == g_q_head) g_q_head = (g_q_head + 1) & 3; // drop oldest
}

// Minimal IPv4 + ICMP parser to capture echo replies
void rawicmp_on_ipv4_packet(const kos::common::uint8_t* data, kos::common::uint32_t len) {
    if (len < 20) return; // IPv4 header min
    const kos::common::uint8_t ihl = (data[0] & 0x0F) * 4;
    if (ihl < 20 || len < ihl) return;
    const kos::common::uint8_t proto = data[9];
    if (proto != 1) return; // ICMP
    const kos::common::uint8_t* icmp = data + ihl;
    kos::common::uint32_t icmp_len = len - ihl;
    if (icmp_len < 8) return; // ICMP header min
    const kos::common::uint8_t type = icmp[0];
    const kos::common::uint8_t code = icmp[1];
    (void)code;
    if (type != 0) return; // Echo Reply
    kos::common::uint16_t ident = (icmp[4] << 8) | icmp[5];
    kos::common::uint16_t seq   = (icmp[6] << 8) | icmp[7];
    kos::common::uint32_t pay_len = icmp_len - 8;
    if (pay_len > 128) pay_len = 128;
    queue_reply(ident, seq, icmp + 8, pay_len);
}

kos::common::uint32_t rawicmp_recv_echo(
    RawIcmpHandle /*h*/,
    kos::common::uint16_t id,
    kos::common::uint16_t seq,
    kos::common::uint8_t* buf,
    kos::common::uint32_t buf_len,
    kos::common::uint32_t timeout_ms) {
    // Simple busy-wait polling loop (placeholder without timer)
    // Attempts to find matching (id,seq) in queue.
    // timeout_ms is ignored; return immediately if present.
    int qi = g_q_head;
    while (qi != g_q_tail) {
        EchoReply* e = &g_queue[qi];
        if (e->id == id && e->seq == seq) {
            kos::common::uint32_t n = (e->len < buf_len) ? e->len : buf_len;
            for (kos::common::uint32_t i = 0; i < n; ++i) buf[i] = e->buf[i];
            // remove from queue by advancing head
            g_q_head = (qi + 1) & 3;
            return n;
        }
        qi = (qi + 1) & 3;
    }
    return 0;
}

}
}
