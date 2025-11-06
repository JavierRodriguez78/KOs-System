# Networking in KOS (stub)

This kernel currently lacks a real NIC driver and TCP/IP stack. The provided `NetworkManager` kernel service focuses on configuration only:

- Reads `/etc/network.cfg` (or `/ETC/NETWORK.CFG`) with keys: IFNAME, MODE, IP, MASK, GW, DNS.
- If `MODE=dhcp`, it fills well-known QEMU NAT defaults when values are missing:
  - IP: 10.0.2.15, MASK: 255.255.255.0, GW: 10.0.2.2, DNS: 10.0.2.3.
- Persists state for userland in:
  - `/var/run/net/if0.addr`, `/var/run/net/gw`, `/var/run/net/dns` and a minimal `/etc/resolv.conf`.
- Logs configuration to the journal (if enabled) and console.

Enabling the service:

- Toggle in `/etc/services.cfg` (case-insensitive on FAT): `service.network=on`.
- Default config is provided at `disk/etc/NETWORK.CFG`.

Limitations (current):

- No Ethernet driver (e1000/rtl8139) is implemented.
- No IPv4/ARP/ICMP/UDP/TCP stack is present. The `lib/socket` API is a local stub for intra-kernel message passing only.
- As a result, obtaining a real DHCP lease or Internet connectivity is not yet possible.

Roadmap suggestions:

1) Implement a NIC driver (suggested for QEMU):
   - e1000 (82540EM) or RTL8139 are common and well-documented.
2) Integrate a lightweight IP stack (e.g., lwIP):
   - Start with IPv4 + ARP + ICMP + UDP; add TCP later.
   - Provide a minimal socket shim to map to `kos::lib::Socket`.
3) DHCP client (userland or kernel service):
   - Use UDP/BOOTP over raw Ethernet to obtain lease.
4) Routing and DNS:
   - Default route via GW from DHCP; write `/etc/resolv.conf` as done here.

Once (1)+(2) exist, `NetworkManager` can evolve from a stub to actually bringing the link up, running DHCP, and publishing the lease.