#pragma once

#include <common/types.hpp>

namespace kos {
namespace sys {

// Network configuration structure
struct NetConfig {
    char ip[16];
    char mask[16];
    char gateway[16];
    char dns[16];
};

// Initialize the syscall table (kernel only)
void InitializeSyscallTable();

} // namespace sys
} // namespace kos

// System call interface (C linkage for use from both kernel and userspace)
extern "C" {
    int kos_sys_syscall_get_net_config(kos::sys::NetConfig* config);
    int kos_sys_syscall_get_mac_address(kos::common::uint8_t mac[6]);
    int kos_sys_syscall_send_ethernet_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len);
}
