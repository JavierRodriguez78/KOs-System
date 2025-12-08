#include <services/network_manager.hpp>
#include <console/logger.hpp>
#include <fs/filesystem.hpp>
#include <lib/string.hpp>
#include "include/net/rx_dispatch.hpp"
#include <memory/heap.hpp>
#include <process/thread_manager.hpp>
#include <net/ipv4.hpp>
#include <net/interface.hpp>
#include "include/net/nic.hpp"

using namespace kos::console;
using namespace kos::memory;
using namespace kos::lib;
using namespace kos::services;
using namespace kos::process;   
using namespace kos::net::ipv4;
using namespace kos::net::iface;



// Helper at global scope to access the filesystem pointer defined in kernel.cpp
extern kos::fs::Filesystem* get_fs();

static void lc(char* s) { while (*s) { if (*s>='A' && *s<='Z') *s = char(*s - 'A' + 'a'); ++s; } }
static void trimr(char* s) { int l = (int)String::strlen((const int8_t*)s); while (l>0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r')) s[--l]=0; }

bool NetworkManagerService::loadConfig() {
    // Defaults
    cfg_.ifname[0] = 0; cfg_.mode[0] = 0; cfg_.ip[0] = 0; cfg_.mask[0] = 0; cfg_.gw[0] = 0; cfg_.dns[0] = 0;
    String::strncpy((int8_t*)cfg_.ifname, (const int8_t*)"eth0", sizeof(cfg_.ifname)-1);
    String::strncpy((int8_t*)cfg_.mode, (const int8_t*)"dhcp", sizeof(cfg_.mode)-1);

    auto fs = get_fs();
    if (!fs) return true; // no FS: keep defaults

    const int8_t* paths[] = {
        (const int8_t*)"/etc/network.cfg",
        (const int8_t*)"/ETC/NETWORK.CFG",
    };

    const uint32_t MAX_CFG = 2048;
    uint8_t* buf = (uint8_t*)Heap::Alloc(MAX_CFG);
    if (!buf) return true;
    int32_t n = -1; const int8_t* used = nullptr;
    for (unsigned i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
        n = fs->ReadFile(paths[i], buf, MAX_CFG-1);
        if (n > 0) { used = paths[i]; break; }
    }
    if (n <= 0) { Heap::Free(buf); return true; }
    buf[n] = 0;

    Logger::LogKV("Network: using config", (const char*)used);

    char* cur = (char*)buf;
    while (*cur) {
        char* line = cur;
        char* nl = (char*)String::memchr((uint8_t*)cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += String::strlen((const int8_t*)cur); }

        // skip spaces and comments
        char* p = line; while (*p==' '||*p=='\t'||*p=='\r') ++p; if (*p==0 || *p=='#') continue;
        char* eq = (char*)String::memchr((uint8_t*)p, '=', (uint32_t)String::strlen((const int8_t*)p));
        if (!eq) continue; *eq = 0; char* key = p; char* val = eq + 1; trimr(key); while (*val==' '||*val=='\t') ++val; trimr(val);
        lc(key); lc(val);
        if (String::strcmp((const uint8_t*)key, (const uint8_t*)"ifname") == 0) {
            String::strncpy((int8_t*)cfg_.ifname, (const int8_t*)val, sizeof(cfg_.ifname)-1);
        } else if (String::strcmp((const uint8_t*)key, (const uint8_t*)"mode") == 0) {
            String::strncpy((int8_t*)cfg_.mode, (const int8_t*)val, sizeof(cfg_.mode)-1);
        } else if (String::strcmp((const uint8_t*)key, (const uint8_t*)"ip") == 0) {
            String::strncpy((int8_t*)cfg_.ip, (const int8_t*)val, sizeof(cfg_.ip)-1);
        } else if (String::strcmp((const uint8_t*)key, (const uint8_t*)"mask") == 0 || String::strcmp((const uint8_t*)key, (const uint8_t*)"netmask") == 0) {
            String::strncpy((int8_t*)cfg_.mask, (const int8_t*)val, sizeof(cfg_.mask)-1);
        } else if (String::strcmp((const uint8_t*)key, (const uint8_t*)"gw") == 0 || String::strcmp((const uint8_t*)key, (const uint8_t*)"gateway") == 0) {
            String::strncpy((int8_t*)cfg_.gw, (const int8_t*)val, sizeof(cfg_.gw)-1);
        } else if (String::strcmp((const uint8_t*)key, (const uint8_t*)"dns") == 0) {
            String::strncpy((int8_t*)cfg_.dns, (const int8_t*)val, sizeof(cfg_.dns)-1);
        }
    }

    Heap::Free(buf);
    return true;
}

bool NetworkManagerService::tryDhcpStub() {
    // This is a placeholder that assumes QEMU user-mode defaults.
    // If mode=dhcp or no static IP provided, fill with common NAT defaults.
    if (cfg_.mode[0] == 0) String::strncpy((int8_t*)cfg_.mode, (const int8_t*)"dhcp", sizeof(cfg_.mode)-1);
    if (String::strcmp((const uint8_t*)cfg_.mode, (const uint8_t*)"dhcp") != 0) return true; // not DHCP

    bool missing = (cfg_.ip[0]==0) || (cfg_.mask[0]==0) || (cfg_.gw[0]==0) || (cfg_.dns[0]==0);
    if (!missing) return true;

    // Fill in defaults only if missing
    if (cfg_.ip[0]==0)   String::strncpy((int8_t*)cfg_.ip,   (const int8_t*)"10.0.2.15", sizeof(cfg_.ip)-1);
    if (cfg_.mask[0]==0) String::strncpy((int8_t*)cfg_.mask, (const int8_t*)"255.255.255.0", sizeof(cfg_.mask)-1);
    if (cfg_.gw[0]==0)   String::strncpy((int8_t*)cfg_.gw,   (const int8_t*)"10.0.2.2", sizeof(cfg_.gw)-1);
    if (cfg_.dns[0]==0)  String::strncpy((int8_t*)cfg_.dns,  (const int8_t*)"10.0.2.3", sizeof(cfg_.dns)-1);
    return true;
}

void NetworkManagerService::logConfig(const char* prefix) {
    Logger::Log(prefix);
    Logger::LogKV("net.if", cfg_.ifname);
    Logger::LogKV("net.mode", cfg_.mode);
    Logger::LogKV("net.ip", cfg_.ip);
    Logger::LogKV("net.mask", cfg_.mask);
    Logger::LogKV("net.gw", cfg_.gw);
    Logger::LogKV("net.dns", cfg_.dns);
}

void NetworkManagerService::writeStateFiles() {
    auto fs = get_fs(); if (!fs) return;
    // Ensure directories exist (best effort)
    fs->Mkdir((const int8_t*)"/VAR/RUN", 1);
    fs->Mkdir((const int8_t*)"/VAR/RUN/NET", 1);

    const int8_t* ipPath = (const int8_t*)"/var/run/net/if0.addr";
    const int8_t* gwPath = (const int8_t*)"/var/run/net/gw";
    const int8_t* dnsPath = (const int8_t*)"/var/run/net/dns";
    // Write content (each followed by \n)
    if (cfg_.ip[0])  { fs->WriteFile(ipPath,  (const uint8_t*)cfg_.ip,  kos::lib::String::strlen((const int8_t*)cfg_.ip)); fs->WriteFile(ipPath, (const uint8_t*)"\n", 1); }
    if (cfg_.gw[0])  { fs->WriteFile(gwPath,  (const uint8_t*)cfg_.gw,  kos::lib::String::strlen((const int8_t*)cfg_.gw)); fs->WriteFile(gwPath, (const uint8_t*)"\n", 1); }
    if (cfg_.dns[0]) { fs->WriteFile(dnsPath, (const uint8_t*)cfg_.dns, kos::lib::String::strlen((const int8_t*)cfg_.dns)); fs->WriteFile(dnsPath,(const uint8_t*)"\n", 1); }

    // Also create a minimal resolv.conf
    const int8_t* resolv = (const int8_t*)"/etc/resolv.conf";
    if (cfg_.dns[0]) {
        const char* head = "nameserver ";
        fs->WriteFile(resolv, (const uint8_t*)head, (uint32_t)kos::lib::String::strlen((const int8_t*)head));
        fs->WriteFile(resolv, (const uint8_t*)cfg_.dns, (uint32_t)kos::lib::String::strlen((const int8_t*)cfg_.dns));
        fs->WriteFile(resolv, (const uint8_t*)"\n", 1);
    }
}

bool NetworkManagerService::Start() {
    Logger::Log("NetworkManager: starting");
    loadConfig();
    // If DHCP requested, fill known defaults (stub). Real DHCP requires NIC + UDP/IP.
    tryDhcpStub();
    // If still missing configuration, apply a simple static default suitable for bridged networks
    if (cfg_.ip[0] == 0 || cfg_.mask[0] == 0 || cfg_.gw[0] == 0) {
        kos::lib::String::strncpy((int8_t*)cfg_.ip,   (const int8_t*)"192.168.1.50", sizeof(cfg_.ip)-1);
        kos::lib::String::strncpy((int8_t*)cfg_.mask, (const int8_t*)"255.255.255.0", sizeof(cfg_.mask)-1);
        kos::lib::String::strncpy((int8_t*)cfg_.gw,   (const int8_t*)"192.168.1.1", sizeof(cfg_.gw)-1);
        if (cfg_.dns[0] == 0) {
            kos::lib::String::strncpy((int8_t*)cfg_.dns,  (const int8_t*)"8.8.8.8", sizeof(cfg_.dns)-1);
        }
        kos::lib::String::strncpy((int8_t*)cfg_.mode, (const int8_t*)"static", sizeof(cfg_.mode)-1);
        Logger::Log("NetworkManager: applied static defaults");
    }
    logConfig("NetworkManager: configured");
    writeStateFiles();

    // Publish config into in-kernel IPv4 stub so future stack can read it
    Config ipcfg{};
    String::strncpy((int8_t*)ipcfg.ip,   (const int8_t*)cfg_.ip,   sizeof(ipcfg.ip)-1);
    String::strncpy((int8_t*)ipcfg.mask, (const int8_t*)cfg_.mask, sizeof(ipcfg.mask)-1);
    String::strncpy((int8_t*)ipcfg.gw,   (const int8_t*)cfg_.gw,   sizeof(ipcfg.gw)-1);
    String::strncpy((int8_t*)ipcfg.dns,  (const int8_t*)cfg_.dns,  sizeof(ipcfg.dns)-1);
    SetConfig(ipcfg);

    // Publish a minimal interface state for eth0
    Interface ifc{};
    kos::lib::String::strncpy((int8_t*)ifc.name, (const int8_t*)cfg_.ifname, sizeof(ifc.name)-1);
    // Try to read MAC from NIC layer if available
    {
        uint8_t macb[6];
        if (kos_nic_get_mac(macb)) {
            char macstr[18];
            // Manually format MAC as xx:xx:xx:xx:xx:xx
            const char hexchars[] = "0123456789abcdef";
            int pos = 0;
            for (int i = 0; i < 6; ++i) {
                uint8_t b = macb[i];
                macstr[pos++] = hexchars[(b >> 4) & 0xF];
                macstr[pos++] = hexchars[b & 0xF];
                if (i != 5) macstr[pos++] = ':';
            }
            macstr[pos] = '\0';
            kos::lib::String::strncpy((int8_t*)ifc.mac, (const int8_t*)macstr, sizeof(ifc.mac)-1);
        } else {
            kos::lib::String::strncpy((int8_t*)ifc.mac, (const int8_t*)"00:00:00:00:00:00", sizeof(ifc.mac)-1);
        }
    }
    ifc.mtu = 1500;
    ifc.up = true;        // configuration applied
    ifc.running = true;   // mark running once NIC is present; will reflect link later
    ifc.rx_packets = 0;
    ifc.rx_bytes = 0;
    ifc.tx_packets = 0;
    ifc.tx_bytes = 0;
    SetInterface(ifc);

    // Initialize NIC RX dispatch to route frames to ARP/IP/ICMP handlers
    kos::net::rx_dispatch_init();
    // Without a NIC driver/stack we cannot actually bring up a link; report capability
    Logger::Log("NetworkManager: NOTE: NIC driver and TCP/IP stack not present yet; internet connectivity not available");

    // Spawn a lightweight worker thread so service is visible in 'top'
    if (threadId_ == 0 && kos::process::g_thread_manager) {
        s_self = this;
        threadId_ = kos::process::ThreadManagerAPI::CreateSystemThread((void*)worker_trampoline,
                        kos::process::THREAD_SYSTEM_SERVICE, 2048, kos::process::PRIORITY_LOW, "networkd");
        if (threadId_) Logger::LogKV("NetworkManager: worker thread", "started");
    }
    return true;
}

// Static members
NetworkManagerService* NetworkManagerService::s_self = nullptr;

void NetworkManagerService::worker_trampoline() {
    // Keep a heartbeat so it shows in top; later this can monitor link or DHCP lease
    for (;;) {
        SchedulerAPI::SleepThread(2000);
    }
}



// --- Minimal interface storage ---
namespace {
    kos::net::iface::Interface g_ifc{};
}

namespace kos { namespace net { namespace iface {
    void SetInterface(const Interface& ifc) { g_ifc = ifc; }
    const Interface& GetInterface() { return g_ifc; }
}}}


