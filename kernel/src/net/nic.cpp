#include "include/net/nic.hpp"

static void (*_rx_cb)(const kos::common::uint8_t*, kos::common::uint32_t) = 0;
static bool (*_tx_fn)(const kos::common::uint8_t*, kos::common::uint32_t) = 0;
static kos::common::uint8_t _mac[6] = {0,0,0,0,0,0};
static int _mac_set = 0;

extern "C" {

void kos_nic_register_rx(void (*cb)(const kos::common::uint8_t* frame, kos::common::uint32_t len)) {
    _rx_cb = cb;
}

bool kos_nic_send_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    if (_tx_fn) return _tx_fn(frame, len);
    return false;
}
void kos_nic_register_tx(bool (*txfn)(const kos::common::uint8_t* frame, kos::common::uint32_t len)) {
    _tx_fn = txfn;
}

void kos_nic_driver_rx(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    if (_rx_cb) _rx_cb(frame, len);
}

void kos_nic_set_mac(const kos::common::uint8_t mac[6]) {
    for (int i = 0; i < 6; ++i) _mac[i] = mac[i];
    _mac_set = 1;
}

bool kos_nic_get_mac(kos::common::uint8_t mac_out[6]) {
    if (!_mac_set) return false;
    for (int i = 0; i < 6; ++i) mac_out[i] = _mac[i];
    return true;
}


}
