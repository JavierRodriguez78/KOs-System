#include "include/net/raw_icmp.hpp"
#include "include/net/icmp.hpp"
#include "include/net/ipv4_packet.hpp"
#include "include/net/arp.hpp"
#include "include/net/ipv4.hpp"
#include "include/common/types.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"
#include <lib/libc/stdio.h>
#include <lib/syscalls.hpp>

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
    kos_printf((const int8_t*)"[raw_icmp] Sending echo request...\n");
    
    // Build ICMP Echo Request payload
    kos::common::uint8_t icmp_buf[64];
    kos::common::uint32_t icmp_len = kos::net::icmp::build_echo_request(id, seq, payload, payload_len, icmp_buf, sizeof(icmp_buf));
    if (icmp_len == 0) {
        kos_printf((const int8_t*)"[raw_icmp] Failed to build ICMP packet\n");
        return false;
    }

    // Get network configuration from kernel via syscall
    kos::sys::NetConfig netcfg{};
    if (kos_sys_syscall_get_net_config(&netcfg) < 0) {
        kos_printf((const int8_t*)"[raw_icmp] Failed to get network config from kernel\n");
        return false;
    }
    
    kos_printf((const int8_t*)"[raw_icmp] Source IP from kernel: %s\n", netcfg.ip);
    
    // Build IPv4 packet around ICMP
    kos::common::uint8_t ip_buf[96];
    
    // Parse source IP to big-endian uint32
    unsigned parts[4] = {0,0,0,0}; int pi = 0; unsigned val = 0;
    for (int i = 0; netcfg.ip[i] != '\0'; ++i) {
        char ch = netcfg.ip[i];
        if (ch >= '0' && ch <= '9') { val = val*10 + (unsigned)(ch-'0'); if (val>255) { val=0; break; } }
        else if (ch == '.') { parts[pi++] = val; val = 0; if (pi>3) break; }
        else { break; }
    }
    if (pi == 3) parts[3] = val; else { parts[0]=0; parts[1]=0; parts[2]=0; parts[3]=0; }
    kos::common::uint32_t src_ip_be = (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8)|parts[3];
    kos_printf((const int8_t*)"[raw_icmp] Building IP packet, src=%u.%u.%u.%u\n",
               parts[0], parts[1], parts[2], parts[3]);
    
    kos::common::uint32_t ip_len = build_ipv4_packet(
        icmp_buf, icmp_len,
        /*src_ip_be*/src_ip_be,
        dst_ip_be,
        /*protocol*/1, /* ICMP */
        /*ttl*/64,
        ip_buf, sizeof(ip_buf));
    if (ip_len == 0) {
        kos_printf((const int8_t*)"[raw_icmp] ERROR: Failed to build IP packet\n");
        return false;
    }
    kos_printf((const int8_t*)"[raw_icmp] IP packet built, len=%u\n", ip_len);

    // Resolve MAC via ARP (fallback to broadcast)
    kos::net::MacAddr mac{};
    bool have_mac = arp_resolve(kos::net::IPv4Addr{dst_ip_be}, mac);
    kos_printf((const int8_t*)"[raw_icmp] ARP resolution: %s\n", have_mac ? "SUCCESS" : "BROADCAST");

    // Build Ethernet frame
    kos::common::uint8_t frame[128];
    if (ip_len + sizeof(kos::net::EthernetHeader) > sizeof(frame)) {
        kos_printf((const int8_t*)"[raw_icmp] ERROR: Frame too large (%u > 128)\n",
                   ip_len + sizeof(kos::net::EthernetHeader));
        return false;
    }
    kos_printf((const int8_t*)"[raw_icmp] Building Ethernet frame...\n");
    kos::net::EthernetHeader* eth = reinterpret_cast<kos::net::EthernetHeader*>(frame);
    // Destination MAC
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = have_mac ? mac.b[i] : 0xFF;
    }
    
    // Get source MAC from kernel via syscall
    kos::common::uint8_t smac[6];
    bool have_src_mac = (kos_sys_syscall_get_mac_address(smac) == 0);
    
    if (have_src_mac) {
        for (int i = 0; i < 6; ++i) eth->src[i] = smac[i];
        kos_printf((const int8_t*)"[raw_icmp] Source MAC from kernel: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   smac[0], smac[1], smac[2], smac[3], smac[4], smac[5]);
    } else {
        kos_printf((const int8_t*)"[raw_icmp] WARNING: No MAC from kernel, using default\n");
        smac[0] = 0x52; smac[1] = 0x54; smac[2] = 0x00;
        smac[3] = 0x12; smac[4] = 0x34; smac[5] = 0x56;
        for (int i = 0; i < 6; ++i) eth->src[i] = smac[i];
    }
    eth->ethertype = kos::net::ETHERTYPE_IPV4;

    // Copy IP payload after header
    kos::common::uint8_t* out = frame + sizeof(kos::net::EthernetHeader);
    for (kos::common::uint32_t i = 0; i < ip_len; ++i) out[i] = ip_buf[i];

    // Send via kernel syscall instead of direct NIC call
    kos::common::uint32_t frame_len = sizeof(kos::net::EthernetHeader) + ip_len;
    int sent_bytes = kos_sys_syscall_send_ethernet_frame(frame, frame_len);
    bool sent = (sent_bytes > 0);
    
    // Debug: Print result to serial/stdout
    if (sent) {
        kos_printf((const int8_t*)"[raw_icmp] Frame sent via kernel, len=%u\n", frame_len);
    } else {
        kos_printf((const int8_t*)"[raw_icmp] Frame send FAILED (kernel returned %d)\n", sent_bytes);
    }
    
    return sent;
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
