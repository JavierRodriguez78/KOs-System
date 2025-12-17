# Networking in KOS

## Current Status

The kernel now includes:
- ✅ **NetworkManager service** - Configuration and orchestration (registered in kernel services)
- ✅ **E1000 driver** - Basic hardware detection and MMIO mapping
- ✅ **Network stack components** - IPv4, ARP, ICMP, Ethernet frame handling
- ✅ **RX dispatch** - Packet routing to protocol handlers
- ⚠️ **Partial TX/RX** - Infrastructure present but descriptor rings not yet implemented

## Service Configuration

The `NetworkManager` kernel service handles network configuration:

- Reads `/etc/network.cfg` (or `/ETC/NETWORK.CFG`) with keys: IFNAME, MODE, IP, MASK, GW, DNS.
- If `MODE=dhcp`, it fills well-known QEMU NAT defaults when values are missing:
  - IP: 10.0.2.15, MASK: 255.255.255.0, GW: 10.0.2.2, DNS: 10.0.2.3.
- Persists state for userland in:
  - `/var/run/net/if0.addr`, `/var/run/net/gw`, `/var/run/net/dns` and a minimal `/etc/resolv.conf`.
- Logs configuration to the journal (if enabled) and console.

Enabling the service:

- Toggle in `/etc/services.cfg` (case-insensitive on FAT): `service.network=on`.
- Default config is provided at `disk/etc/NETWORK.CFG`.

## Current Limitations

### Hardware Drivers
- ✅ E1000 (82540EM) - Device detection and MMIO mapping implemented
- ⚠️ TX/RX descriptor rings not yet implemented
- ❌ RTL8139/RTL8169/RTL8822be - Only probe stubs exist
- ❌ No interrupt handling for packet reception

### Network Stack  
- ✅ IPv4 packet building and parsing
- ✅ ARP cache and resolution
- ✅ ICMP echo request/reply support
- ✅ Ethernet frame encapsulation
- ❌ No UDP/TCP implementation yet
- ❌ No socket API for userspace

### Applications
- ⚠️ `ping` - Framework ready but requires `KOS_NET_HAVE_RAW_ICMP` flag
- ⚠️ `ifconfig` - Can display config but cannot modify interfaces
- ❌ No DHCP client yet

## What Works Now

1. **Network configuration** - NetworkManager service starts and configures network settings
2. **Hardware detection** - E1000 NIC is detected and MAC address is read
3. **MMIO access** - Driver can read/write E1000 registers
4. **Packet routing** - RX dispatch routes packets to protocol handlers
5. **Protocol handling** - IPv4, ARP, ICMP handlers are in place

## What Doesn't Work Yet

1. **Actual packet transmission** - TX descriptor ring not implemented
2. **Packet reception** - RX descriptor ring and IRQ handler not implemented  
3. **Internet connectivity** - Cannot send/receive real packets
4. **Ping functionality** - Requires packet TX/RX to work

## Next Steps to Enable Internet Connectivity

### HIGH PRIORITY (blocking connectivity):

1. **Implement E1000 TX descriptor ring**
   - Allocate ring buffer in memory
   - Configure TDBAL/TDBAH/TDLEN registers
   - Implement packet queuing in `e1000_tx_impl()`
   - Update TDT register to signal hardware

2. **Implement E1000 RX descriptor ring**
   - Allocate ring buffer and packet buffers
   - Configure RDBAL/RDBAH/RDLEN registers  
   - Register IRQ handler for packet reception
   - Call `e1000_submit_rx_frame()` when packets arrive

3. **Enable interrupts for E1000**
   - Configure IMS register
   - Register IRQ handler in kernel
   - Enable IRQ line in PIC

### MEDIUM PRIORITY (for full functionality):

4. **Define KOS_NET_HAVE_RAW_ICMP**
   - Add to build configuration
   - Test ping command with real hardware

5. **Implement DHCP client**
   - UDP/BOOTP over raw Ethernet
   - Obtain and apply lease

6. **Add TCP/UDP support**
   - Consider integrating lwIP or custom implementation

## Testing Recommendations

When testing networking:

1. **Check logs for**:
   - "NetworkManager: starting"
   - "e1000: device detected"
   - "e1000.mac: XX:XX:XX:XX:XX:XX"
   - "e1000: driver activated successfully"

2. **Use these commands**:
   - `ifconfig` - View network interface status
   - `lshw` - Check hardware detection
   - `ss` - View socket/service status (when implemented)
   - `ping` - Test connectivity (after TX/RX rings implemented)

3. **QEMU network options**:
   ```bash
   # User-mode networking (NAT)
   qemu-system-x86_64 -netdev user,id=net0 -device e1000,netdev=net0
   
   # Bridge mode (requires host setup)
   qemu-system-x86_64 -netdev bridge,id=net0 -device e1000,netdev=net0
   ```

## Architecture Overview

```
Application Layer
    ↓
  ping.c (KOS_NET_HAVE_RAW_ICMP)
    ↓
raw_icmp.cpp (ICMP echo send/receive)
    ↓
ipv4_packet.cpp (IPv4 packet building)
    ↓
ethernet.cpp (Frame encapsulation)
    ↓
nic.cpp (Hardware abstraction)
    ↓
e1000.cpp (Driver implementation)
    ↓
Hardware (Intel 82540EM NIC)
```

## Roadmap

### Phase 1: Basic Connectivity (In Progress)
- [x] E1000 device detection
- [x] MMIO mapping and register access
- [x] MAC address reading
- [ ] TX descriptor ring
- [ ] RX descriptor ring + IRQ
- [ ] Ping command functionality

### Phase 2: Protocol Support
- [ ] UDP implementation
- [ ] DHCP client
- [ ] DNS resolution
- [ ] TCP implementation

### Phase 3: Userspace Integration
- [ ] Socket API for applications
- [ ] Network utilities (netstat, route, etc.)
- [ ] Configuration tools

---

*Last updated: December 2025*
*Status: Active development - Basic hardware support implemented*