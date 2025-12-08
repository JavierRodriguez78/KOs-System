#pragma once

#include "include/common/types.hpp"
using namespace kos::common;

extern "C" {

    // Register a single RX callback that receives raw Ethernet frames.
    // Drivers should call this when a frame arrives.
    void kos_nic_register_rx(void (*cb)(const uint8_t* frame, uint32_t len));

    // Send a raw Ethernet frame via the active NIC.
    // Returns true if queued for transmission.
    bool kos_nic_send_frame(const uint8_t* frame, uint32_t len);
    
    // Drivers register their TX function so the stack can send frames.
    void kos_nic_register_tx(bool (*txfn)(const uint8_t* frame, uint32_t len));

    // Drivers call this to submit a received frame to the stack.
    void kos_nic_driver_rx(const uint8_t* frame, uint32_t len);

        // NIC MAC address management: driver sets, stack reads.
        void kos_nic_set_mac(const uint8_t mac[6]);
        bool kos_nic_get_mac(uint8_t mac_out[6]);

}
