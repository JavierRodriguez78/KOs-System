#include "include/net/arp_cache.hpp"
#include "include/net/ethernet.hpp"
#include "include/net/nic.hpp"

namespace kos { namespace net {

static IPv4Addr _ips[8];
static MacAddr  _macs[8];
static int      _used[8];

bool arp_cache_lookup(const IPv4Addr& ip, MacAddr& mac_out) {
    for (int i = 0; i < 8; ++i) {
        if (_used[i] && _ips[i].addr == ip.addr) { mac_out = _macs[i]; return true; }
    }
    return false;
}

void arp_cache_update(const IPv4Addr& ip, const MacAddr& mac) {
    for (int i = 0; i < 8; ++i) {
        if (_used[i] && _ips[i].addr == ip.addr) { _macs[i] = mac; return; }
    }
    for (int i = 0; i < 8; ++i) {
        if (!_used[i]) { _used[i] = 1; _ips[i] = ip; _macs[i] = mac; return; }
    }
}

} }
