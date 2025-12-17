// Syscall table initialization for kernel
// This sets up function pointers at a fixed address so apps can call kernel functions

#include <lib/syscalls.hpp>
#include <net/ipv4.hpp>
#include <net/nic.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <lib/stdio.hpp>

#define SYSCALL_TABLE_ADDR 0x00100000

struct KernelSyscallTable {
    int (*get_net_config)(kos::sys::NetConfig* config);
    int (*get_mac_address)(kos::common::uint8_t mac[6]);
    int (*send_ethernet_frame)(const kos::common::uint8_t* frame, kos::common::uint32_t len);
};

// Forward declarations of the actual syscall implementations
extern "C" {
    int kos_sys_syscall_get_net_config(kos::sys::NetConfig* config);
    int kos_sys_syscall_get_mac_address(kos::common::uint8_t mac[6]);
    int kos_sys_syscall_send_ethernet_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len);
}

namespace kos {
namespace sys {

void InitializeSyscallTable() {
    KernelSyscallTable* table = (KernelSyscallTable*)SYSCALL_TABLE_ADDR;
    table->get_net_config = kos_sys_syscall_get_net_config;
    table->get_mac_address = kos_sys_syscall_get_mac_address;
    table->send_ethernet_frame = kos_sys_syscall_send_ethernet_frame;
    
    console::Logger::Log("Syscall table initialized at 0x00100000");
}

} // namespace sys
} // namespace kos
