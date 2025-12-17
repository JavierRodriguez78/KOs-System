// Syscall table: fixed memory location where kernel stores function pointers
// Apps can call kernel functions through this table

#include <lib/syscalls.hpp>

// Fixed address for syscall table - must match kernel's definition
#define SYSCALL_TABLE_ADDR 0x00100000

struct KernelSyscallTable {
    int (*get_net_config)(kos::sys::NetConfig* config);
    int (*get_mac_address)(kos::common::uint8_t mac[6]);
    int (*send_ethernet_frame)(const kos::common::uint8_t* frame, kos::common::uint32_t len);
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

} // extern "C"
