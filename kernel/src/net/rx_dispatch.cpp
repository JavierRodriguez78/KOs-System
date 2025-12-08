#include "include/net/rx_dispatch.hpp"
#include "include/net/nic.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/arp.hpp"

namespace kos { namespace net {

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
        // Forward to raw ICMP handler if present
        extern void rawicmp_on_ipv4_packet(const kos::common::uint8_t* data, kos::common::uint32_t len);
        rawicmp_on_ipv4_packet(payload, plen);
        return;
    }
}

void rx_dispatch_init() {
    kos_nic_register_rx(rx_cb);
}

} }
