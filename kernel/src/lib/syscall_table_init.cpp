// Syscall table initialization for kernel
// This sets up function pointers at a fixed address so apps can call kernel functions

#include <lib/syscalls.hpp>
#include <net/ipv4.hpp>
#include <net/nic.hpp>
#include <net/dns.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <lib/stdio.hpp>
#include <drivers/net/e1000/e1000_poll.h>

#define SYSCALL_TABLE_ADDR 0x00100000

struct KernelSyscallTable {
    int (*get_net_config)(kos::sys::NetConfig* config);
    int (*get_mac_address)(kos::common::uint8_t mac[6]);
    int (*send_ethernet_frame)(const kos::common::uint8_t* frame, kos::common::uint32_t len);
    void (*rx_poll)(void);
    int (*dns_resolve)(const char* hostname, char* ip_out, int ip_out_size);
};

// Forward declarations of the actual syscall implementations
extern "C" {
    int kos_sys_syscall_get_net_config(kos::sys::NetConfig* config);
    int kos_sys_syscall_get_mac_address(kos::common::uint8_t mac[6]);
    int kos_sys_syscall_send_ethernet_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len);
    int kernel_dns_resolve(const char* hostname, char* ip_out, int ip_out_size);
}

// Kernel-side DNS resolve implementation
int kernel_dns_resolve(const char* hostname, char* ip_str_out, int ip_str_size) {
    kos::console::Logger::Log("kernel_dns_resolve called");
    kos::console::Logger::LogKV("  hostname", hostname);
    
    if (!hostname || !ip_str_out || ip_str_size < 16) return -1;
    
    uint32_t ip_be = 0;
    bool resolved = kos::net::dns::dns_resolve(hostname, 0, 5000, &ip_be);
    
    kos::console::Logger::LogKV("  resolved", resolved ? "true" : "false");
    
    if (!resolved) return -1;
    
    // Convert IP to dotted decimal string
    unsigned char b1 = (ip_be >> 24) & 0xFF;
    unsigned char b2 = (ip_be >> 16) & 0xFF;
    unsigned char b3 = (ip_be >> 8) & 0xFF;
    unsigned char b4 = ip_be & 0xFF;
    
    // Simple IP formatting
    char* p = ip_str_out;
    auto write_num = [&p](unsigned char n) {
        if (n >= 100) { *p++ = '0' + n/100; n %= 100; *p++ = '0' + n/10; n %= 10; }
        else if (n >= 10) { *p++ = '0' + n/10; n %= 10; }
        *p++ = '0' + n;
    };
    write_num(b1); *p++ = '.';
    write_num(b2); *p++ = '.';
    write_num(b3); *p++ = '.';
    write_num(b4); *p = '\0';
    
    return 0;
}

namespace kos {
namespace sys {

void InitializeSyscallTable() {
    KernelSyscallTable* table = (KernelSyscallTable*)SYSCALL_TABLE_ADDR;
    table->get_net_config = kos_sys_syscall_get_net_config;
    table->get_mac_address = kos_sys_syscall_get_mac_address;
    table->send_ethernet_frame = kos_sys_syscall_send_ethernet_frame;
    table->rx_poll = e1000_rx_poll;
    table->dns_resolve = kernel_dns_resolve;
    
    console::Logger::Log("Syscall table initialized at 0x00100000");
}

} // namespace sys
} // namespace kos
