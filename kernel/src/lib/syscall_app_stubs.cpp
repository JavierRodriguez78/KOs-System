// Syscall table: fixed memory location where kernel stores function pointers
// Apps can call kernel functions through this table

#include <lib/syscalls.hpp>

// Fixed address for syscall table - must match kernel's definition
#define SYSCALL_TABLE_ADDR 0x00100000

struct KernelSyscallTable {
    int (*get_net_config)(kos::sys::NetConfig* config);
    int (*get_mac_address)(kos::common::uint8_t mac[6]);
    int (*send_ethernet_frame)(const kos::common::uint8_t* frame, kos::common::uint32_t len);
    void (*rx_poll)(void);
    int (*dns_resolve)(const char* hostname, char* ip_out, int ip_out_size);
};

// Access the kernel's syscall table
static KernelSyscallTable* get_syscall_table() {
    return (KernelSyscallTable*)SYSCALL_TABLE_ADDR;
}

extern "C" {

int kos_sys_syscall_get_net_config(kos::sys::NetConfig* config) {
    KernelSyscallTable* table = get_syscall_table();
    if (table && table->get_net_config) {
        return table->get_net_config(config);
    }
    // If syscall table not initialized, return error
    return -1;
}

int kos_sys_syscall_get_mac_address(kos::common::uint8_t mac[6]) {
    KernelSyscallTable* table = get_syscall_table();
    if (table && table->get_mac_address) {
        return table->get_mac_address(mac);
    }
    return -1;
}

int kos_sys_syscall_send_ethernet_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    KernelSyscallTable* table = get_syscall_table();
    if (table && table->send_ethernet_frame) {
        return table->send_ethernet_frame(frame, len);
    }
    return -1;
}

void kos_sys_syscall_rx_poll(void) {
    KernelSyscallTable* table = get_syscall_table();
    if (table && table->rx_poll) {
        table->rx_poll();
    }
}

// Provide e1000_rx_poll for app code that calls it directly
void e1000_rx_poll(void) {
    kos_sys_syscall_rx_poll();
}

// DNS resolve syscall - runs entirely in kernel
int kos_dns_resolve(const char* hostname, char* ip_str_out, int ip_str_size) {
    KernelSyscallTable* table = get_syscall_table();
    if (table && table->dns_resolve) {
        return table->dns_resolve(hostname, ip_str_out, ip_str_size);
    }
    return -1;
}

} // extern "C"
