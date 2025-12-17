#include <lib/syscalls.hpp>
#include <lib/string.hpp>

// These are the KERNEL's implementations - we force link to them
// by declaring them as extern
extern "C" {
extern int kos_sys_syscall_get_net_config(kos::sys::NetConfig* config);
extern int kos_sys_syscall_get_mac_address(kos::common::uint8_t mac[6]);
extern int kos_sys_syscall_send_ethernet_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len);
}

// For apps: these just forward to the kernel's implementations
// In a real OS these would be syscall traps, but since KOS has no memory protection
// we can just call the kernel functions directly
namespace kos {
namespace sys {

int GetNetConfig(NetConfig* config) {
    return kos_sys_syscall_get_net_config(config);
}

int GetMacAddress(common::uint8_t mac[6]) {
    return kos_sys_syscall_get_mac_address(mac);
}

int SendEthernetFrame(const common::uint8_t* frame, common::uint32_t len) {
    return kos_sys_syscall_send_ethernet_frame(frame, len);
}

} // namespace sys
} // namespace kos
