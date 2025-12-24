#include <lib/syscalls.hpp>
#include <net/ipv4.hpp>
#include <net/nic.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <lib/stdio.hpp>

using namespace kos;

extern "C" {

// Get network configuration from kernel
int kos_sys_syscall_get_net_config(sys::NetConfig* config) {
    if (!config) return -1;
    
    const auto& cfg = net::ipv4::GetConfig();
    
    // Debug: Log what we're returning
    console::Logger::Log("syscall_get_net_config called");
    console::Logger::LogKV("  Returning IP", cfg.ip);
    console::Logger::LogKV("  Returning Mask", cfg.mask);
    console::Logger::LogKV("  Returning GW", cfg.gw);
    console::Logger::LogKV("  Returning DNS", cfg.dns);
    
    lib::String::strncpy((int8_t*)config->ip, (const int8_t*)cfg.ip, 15);
    lib::String::strncpy((int8_t*)config->mask, (const int8_t*)cfg.mask, 15);
    lib::String::strncpy((int8_t*)config->gateway, (const int8_t*)cfg.gw, 15);
    lib::String::strncpy((int8_t*)config->dns, (const int8_t*)cfg.dns, 15);
    
    config->ip[15] = '\0';
    config->mask[15] = '\0';
    config->gateway[15] = '\0';
    config->dns[15] = '\0';
    
    return 0;
}

// Get MAC address from NIC layer
int kos_sys_syscall_get_mac_address(common::uint8_t mac[6]) {
    console::Logger::Log("syscall_get_mac_address called");
    
    if (!mac) return -1;
    
    if (kos_nic_get_mac(mac)) {
        // Log MAC in hex manually
        char mac_str[32];
        static const char hex[] = "0123456789abcdef";
        for (int i = 0; i < 6; ++i) {
            mac_str[i*3]     = hex[(mac[i] >> 4) & 0xF];
            mac_str[i*3 + 1] = hex[mac[i] & 0xF];
            mac_str[i*3 + 2] = (i < 5) ? ':' : '\0';
        }
        mac_str[17] = '\0';
        console::Logger::LogKV("  Returning MAC", mac_str);
        return 0;
    }
    
    console::Logger::Log("  No MAC available from NIC layer");
    return -1;
}

// Send ethernet frame through kernel network stack
int kos_sys_syscall_send_ethernet_frame(const common::uint8_t* frame, common::uint32_t len) {
    console::Logger::Log("syscall_send_ethernet_frame called");
    
    char buf[32];
    kos::sys::snprintf(buf, sizeof(buf), "%u", len);
    console::Logger::LogKV("  Frame length", buf);
    
    if (!frame || len == 0 || len > 1500) {
        console::Logger::Log("  Invalid parameters");
        return -1;
    }
    
    if (kos_nic_send_frame(frame, len)) {
        console::Logger::Log("  Frame sent successfully");
        return (int)len;
    }
    
    console::Logger::Log("  Frame send failed");
    return -1;
}

} // extern "C"
