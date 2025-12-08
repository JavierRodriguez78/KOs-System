#include <console/tty.hpp>
#include <lib/stdio.hpp>
#include <net/ipv4.hpp>
#include <net/interface.hpp>
#include <application/app.hpp>

using namespace kos::console;
using namespace kos::net::ipv4;

extern "C" void app_ifconfig(void) {
    const Config& cfg = GetConfig();
    const kos::net::iface::Interface& ifc = kos::net::iface::GetInterface();

    TTY tty;
    tty.Write((const int8_t*)ifc.name);
    tty.Write((const int8_t*)"    Link encap:Ethernet  HWaddr ");
    tty.Write((const int8_t*)(ifc.mac[0] ? ifc.mac : "(n/a)"));
    tty.Write((const int8_t*)"\n");

    tty.Write((const int8_t*)"          inet addr: ");
    tty.Write((const int8_t*)cfg.ip);
    tty.Write((const int8_t*)"  Mask: ");
    tty.Write((const int8_t*)cfg.mask);
    tty.Write((const int8_t*)"  Gateway: ");
    tty.Write((const int8_t*)cfg.gw);
    tty.Write((const int8_t*)"\n");

    tty.Write((const int8_t*)"          DNS: ");
    tty.Write((const int8_t*)cfg.dns);
    tty.Write((const int8_t*)"\n");

    // Flags and MTU line
    tty.Write((const int8_t*)"          ");
    tty.Write((const int8_t*)(ifc.up ? "UP " : "DOWN "));
    if (ifc.running) tty.Write((const int8_t*)"RUNNING ");
    tty.Write((const int8_t*)"MTU ");
    kos::sys::printf((const int8_t*)"%u\n", (unsigned)ifc.mtu);

    // RX/TX counters
    tty.Write((const int8_t*)"          RX packets ");
    kos::sys::printf((const int8_t*)"%llu  bytes %llu\n", (unsigned long long)ifc.rx_packets, (unsigned long long)ifc.rx_bytes);
    tty.Write((const int8_t*)"          TX packets ");
    kos::sys::printf((const int8_t*)"%llu  bytes %llu\n", (unsigned long long)ifc.tx_packets, (unsigned long long)ifc.tx_bytes);
}