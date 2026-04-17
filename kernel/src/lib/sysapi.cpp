
#include <lib/stdio.hpp>
#include <lib/libc/stdio.h>
#include <lib/socket.hpp>
#include <console/tty.hpp>
#include <fs/filesystem.hpp>
#include <lib/elfloader.hpp>
#include <memory/pmm.hpp>
#include <memory/heap.hpp>
#include <memory/paging.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_constants.hpp>
#include <arch/x86/hardware/rtc/rtc.hpp>

using namespace kos::sys;
using namespace kos::console;
using namespace kos::common;

// Use fully qualified name for global filesystem pointer
static uint32_t g_list_flags = 0; // flags used by listdir_ex

// Forward declaration for path normalization used by sys_listdir and sys_chdir
static void normalize_abs_path(const int8_t* inPath, const int8_t* cwd, int8_t* outBuf, int outSize);

#ifdef __cplusplus
extern "C" {
#endif
int ps_service_getinfo(char* buffer, int maxlen);
#ifdef __cplusplus
}
#endif
extern "C" int sys_get_process_info(char* buffer, int maxlen) {
    return ps_service_getinfo(buffer, maxlen);
}

// --- Simple app-facing key queue ---
// Keys offered by keyboard handler are enqueued here so apps can poll them.
static volatile int g_app_key_head = 0;
static volatile int g_app_key_tail = 0;
static int8_t g_app_key_buf[256];

extern "C" void sys_offer_key(int8_t c) {
    int next_tail = (g_app_key_tail + 1) & 0xFF;
    if (next_tail == g_app_key_head) {
        // Buffer full: drop key
        return;
    }
    g_app_key_buf[g_app_key_tail] = c;
    g_app_key_tail = next_tail;
}

extern "C" int32_t sys_key_poll(int8_t* out) {
    if (g_app_key_head == g_app_key_tail) return 0;
    int8_t c = g_app_key_buf[g_app_key_head];
    g_app_key_head = (g_app_key_head + 1) & 0xFF;
    if (out) *out = c;
    return 1;
}

extern "C" void sys_putc(int8_t c) { TTY::PutChar(c); }
extern "C" void sys_puts(const int8_t* s) { TTY::Write(s); }
extern "C" void sys_hex(uint8_t v) { TTY::WriteHex(v); }
extern "C" void sys_listroot() { if (kos::fs::g_fs_ptr) kos::fs::g_fs_ptr->ListRoot(); }
extern "C" void sys_clear() { TTY::Clear(); }
extern "C" void sys_set_attr(uint8_t a) { TTY::SetAttr(a); }
extern "C" void sys_set_color(uint8_t fg, uint8_t bg) { TTY::SetColor(fg, bg); }
// PCI config read helper: mediates access to PCI config space ports
extern "C" uint32_t sys_pci_cfg_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    using namespace kos::arch::x86::hardware::pci;
    uint32_t id = PCI_ENABLE_BIT |
                  (((uint32_t)bus & PCI_BUS_MASK) << PCI_BUS_SHIFT) |
                  (((uint32_t)device & PCI_DEVICE_MASK) << PCI_DEVICE_SHIFT) |
                  (((uint32_t)function & PCI_FUNCTION_MASK) << PCI_FUNCTION_SHIFT) |
                  (offset & PCI_REGISTER_MASK);
    uint32_t val;
    asm volatile ("outl %0, %1" :: "a"(id), "Nd"(PCI_COMMAND_PORT));
    asm volatile ("inl %1, %0" : "=a"(val) : "Nd"(PCI_DATA_PORT));
    uint8_t shift = (offset & 3) * 8;
    return val >> shift;
}

// RTC date/time provider for apps
extern "C" void sys_get_datetime(uint16_t* year, uint8_t* month, uint8_t* day,
                                  uint8_t* hour, uint8_t* minute, uint8_t* second) {
    using namespace kos::arch::x86::hardware::rtc;
    DateTime dt; RTC::Read(dt);
    if (year)   *year = dt.year;
    if (month)  *month = dt.month;
    if (day)    *day = dt.day;
    if (hour)   *hour = dt.hour;
    if (minute) *minute = dt.minute;
    if (second) *second = dt.second;
}

// ===========================================================================
//  Battery / Power backend
//  Path 1 — ACPI Embedded Controller (EC)
//    Primary path on any ACPI-capable platform (QEMU default machine, most
//    laptops).  Uses the standard PC/AT EC port pair (0x62 data, 0x66 cmd).
//    EC command 0x80 (RD_EC) reads a single byte register by index.
//
//    ACPI EC Battery register map (QEMU PNP0C0A / real ACPI DSDT):
//      0x00  Battery Status  bit0=present  bit1=charging  bit2=discharging
//      0x01  Battery Rate    signed 16-bit LSB (mW, 2 bytes)
//      0x02  Remaining Cap.  unsigned 16-bit LE (mWh)
//      0x04  Full Cap.       unsigned 16-bit LE (mWh)
//
//  Path 2 — APM 32-bit Protected-Mode Interface (fallback)
//    Available when Multiboot1 flag bit 10 is set and the BIOS supports the
//    32-bit PM connection (ApmTable.flags bit 1).  GRUB connects the APM PM
//    interface and stores segment info in the Multiboot APM table.  Under a
//    flat-model GDT (base=0), the APM entry is callable as a near procedure
//    at the linear address (cseg_base + offset).  GRUB uses a flat-base 0,
//    so linear_entry = apm_offset.
//
//    APM function 0x530A (Get Power Status), BX=0x8001 (all batteries)
//    returns: BH=AC status, BL=battery status, CL=remaining% (0xFF=unknown)
// ===========================================================================

// ---------------------------------------------------------------------------
// APM state — saved during early boot before our GDT reinit
// ---------------------------------------------------------------------------

// APM BIOS table as laid out by Multiboot1
struct ApmTable {
    uint16_t version;       // BCD: 0x0102 → v1.2
    uint16_t cseg;          // 32-bit PM code segment (GRUB GDT selector)
    uint32_t offset;        // Entry-point offset within cseg
    uint16_t cseg_16;       // 16-bit PM code segment
    uint16_t dseg;          // Data segment (GRUB GDT selector)
    uint16_t flags;         // bit0=16-bit PM, bit1=32-bit PM, bit2=idle slows CPU
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
} __attribute__((packed));

static bool     g_apm_present  = false;
static uint32_t g_apm_entry    = 0;    // linear address; callable after flat-model GDT
static uint16_t g_apm_dseg_sel = 0;    // original GRUB data-segment selector

// sys_init_power — called from kernelMain() with raw Multiboot info.
// Must be called early (after mb.Init() and GDT, but before first battery
// read) so APM state is captured.
extern "C" void sys_init_power(const void* mb_info, uint32_t magic) {
    if (magic != 0x2BADB002u || !mb_info) return;

    // Multiboot1 flags word (first dword of the info struct)
    const uint32_t mb_flags = *(const uint32_t*)mb_info;
    if (!(mb_flags & (1u << 10u))) return;  // APM table absent

    // apm_table field sits at byte offset 0x44 in the Multiboot1 info struct:
    //  flags(4)+mem_lower(4)+mem_upper(4)+boot_device(4)+cmdline(4)
    //  +mods_count(4)+mods_addr(4)+syms[4](16)+mmap_length(4)+mmap_addr(4)
    //  +drives_length(4)+drives_addr(4)+config_table(4)+boot_loader_name(4) = 0x44
    const uint32_t apm_phys = *(const uint32_t*)((uintptr_t)mb_info + 0x44u);
    if (!apm_phys) return;

    const ApmTable* apt = (const ApmTable*)(uintptr_t)apm_phys;

    // Require 32-bit PM interface (flags bit 1)
    if (!(apt->flags & 0x02u)) return;
    // Sanity: version must be ≥ 1.1
    if (apt->version < 0x0101u) return;

    // GRUB sets up APM with a flat (base=0) code segment, so the entry's
    // linear address equals the raw offset field.
    g_apm_entry    = apt->offset;
    g_apm_dseg_sel = apt->dseg;
    g_apm_present  = true;
}

// ---------------------------------------------------------------------------
// APM Get-Power-Status call (32-bit PM, function 0x530A)
// Returns remaining percentage (0–100) or -1 on failure/unsupported.
// ---------------------------------------------------------------------------
static int32_t _apm_get_battery_percent() {
    if (!g_apm_present || !g_apm_entry) return -1;

    // APM 32-bit PM function 0x530A — Get Power Status
    // Input:  AH=0x53, AL=0x0A, BX=0x8001 (all batteries)
    // Output: BH=AC line status, BL=battery status,
    //         CH=battery flags, CL=remaining life % (0xFF=unknown),
    //         DX=remaining life in seconds or minutes
    //         CF set on error (AH=error code)
    //
    // We make a near call to the flat-model entry (base=0 assumed by GRUB).
    // All GPRs may be clobbered by the APM BIOS; we declare them as clobbers.

    uint32_t entry = g_apm_entry;
    uint8_t  cl_out = 0xFFu;
    uint8_t  cf_out = 1u;

        // ESI and EDI are saved/restored by the asm block itself so we can use
        // them as pointer output registers without including them in clobbers.
        // EAX, EBX, ECX, EDX are freely clobbered by the APM BIOS callee.
        uint8_t* p_cf = &cf_out;
        uint8_t* p_cl = &cl_out;
    __asm__ __volatile__(
            "pushl %%esi         \n\t"   // save ESI (will hold p_cf)
            "pushl %%edi         \n\t"   // save EDI (will hold p_cl)
            "movl  %1, %%esi     \n\t"   // ESI = p_cf
            "movl  %2, %%edi     \n\t"   // EDI = p_cl
            "movw  $0x530A, %%ax \n\t"   // AH=0x53 (APM), AL=0x0A (Get Power Status)
            "movw  $0x8001, %%bx \n\t"   // BX=0x8001 (all batteries)
            "call  *%0           \n\t"   // near call to APM PM entry
            "setc  %%al          \n\t"   // CF → AL
            "movb  %%al, (%%esi) \n\t"   // *p_cf = carry
            "movb  %%cl, (%%edi) \n\t"   // *p_cl = remaining %
            "popl  %%edi         \n\t"   // restore EDI
            "popl  %%esi         \n\t"   // restore ESI
            :
            : "m"(entry), "m"(p_cf), "m"(p_cl)
            : "eax", "ebx", "ecx", "edx", "memory"
    );

    if (cf_out) return -1;           // APM returned an error
    if (cl_out == 0xFFu) return -1;  // percentage unknown
    if (cl_out > 100u) return -1;    // bogus value
    return (int32_t)cl_out;
}

// ---------------------------------------------------------------------------
// ACPI EC port helpers
// ---------------------------------------------------------------------------

// Probe EC presence: if status port reads 0xFF the EC hardware is absent.
static bool _ec_present() {
    uint8_t st;
    __asm__ __volatile__("inb %1, %0" : "=a"(st) : "Nd"((uint16_t)0x66));
    return (st != 0xFFu);
}

static inline void _ec_wait_ibf() {
    uint32_t to = 0x10000u;
    while (to--) {
        uint8_t st;
        __asm__ __volatile__("inb %1, %0" : "=a"(st) : "Nd"((uint16_t)0x66));
        if (!(st & 0x02u)) return;
    }
}

static inline bool _ec_wait_obf() {
    uint32_t to = 0x10000u;
    while (to--) {
        uint8_t st;
        __asm__ __volatile__("inb %1, %0" : "=a"(st) : "Nd"((uint16_t)0x66));
        if (st & 0x01u) return true;
    }
    return false; // timed out — EC not responding
}

// Returns 0xFF on timeout (invalid/not-present indicator).
static uint8_t _ec_read(uint8_t reg) {
    _ec_wait_ibf();
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x80), "Nd"((uint16_t)0x66)); // RD_EC
    _ec_wait_ibf();
    __asm__ __volatile__("outb %0, %1" : : "a"(reg),           "Nd"((uint16_t)0x62));
    if (!_ec_wait_obf()) return 0xFFu;
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"((uint16_t)0x62));
    return val;
}

// ACPI EC battery read. Returns 0–100 or -1.
static int32_t _acpi_ec_battery_percent() {
    if (!_ec_present()) return -1;

    // Battery Status byte: bit 0 = present
    uint8_t bst = _ec_read(0x00u);
    if (bst == 0xFFu) return -1;        // EC timed out
    if (!(bst & 0x01u)) return -1;      // no battery

    // Remaining capacity — 16-bit LE at registers 0x02/0x03
    uint8_t rem_lo = _ec_read(0x02u);
    uint8_t rem_hi = _ec_read(0x03u);
    if (rem_lo == 0xFFu || rem_hi == 0xFFu) return -1;

    // Full charge capacity — 16-bit LE at registers 0x04/0x05
    uint8_t ful_lo = _ec_read(0x04u);
    uint8_t ful_hi = _ec_read(0x05u);
    if (ful_lo == 0xFFu || ful_hi == 0xFFu) return -1;

    uint16_t remaining = (uint16_t)rem_lo | ((uint16_t)rem_hi << 8u);
    uint16_t full      = (uint16_t)ful_lo | ((uint16_t)ful_hi << 8u);
    if (full == 0u) return -1;

    // Sanity: remaining must not exceed full
    if (remaining > full) remaining = full;

    int32_t pct = (int32_t)(((uint32_t)remaining * 100u) / (uint32_t)full);
    if (pct > 100) pct = 100;
    return pct;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
extern "C" int32_t sys_get_battery_percent() {
    // Try ACPI EC first (reliable on QEMU and most ACPI laptops)
    int32_t pct = _acpi_ec_battery_percent();
    if (pct >= 0) return pct;

    // Fallback: APM 32-bit PM interface (older systems / no ACPI battery)
    return _apm_get_battery_percent();
}
extern "C" void sys_listdir(const int8_t* path) {
    if (!kos::fs::g_fs_ptr) return;
    // Resolve relative path against current working directory and normalize
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* use = path && path[0] ? path : cwd;
    normalize_abs_path(use, cwd, absBuf, (int)sizeof(absBuf));
    // Treat any form of root ("/" or only slashes) as ListRoot
    bool isRoot = (absBuf[0] == '/') && (absBuf[1] == 0);
    if (isRoot) { kos::fs::g_fs_ptr->ListRoot(); return; }
    // If the path doesn't exist as a directory, report a clear error up-front
    if (!kos::fs::g_fs_ptr->DirExists(absBuf)) {
        // As a safety net, if it somehow normalizes to root but DirExists disagrees, still list root
    if (absBuf[0] == '/' && absBuf[1] == 0) { kos::fs::g_fs_ptr->ListRoot(); return; }
        TTY::Write((const int8_t*)"ls: path not found: ");
        TTY::Write(absBuf);
        TTY::PutChar('\n');
        return;
    }
    kos::fs::g_fs_ptr->ListDir(absBuf);
}

extern "C" void sys_listdir_ex(const int8_t* path, uint32_t flags) {
    if (!kos::fs::g_fs_ptr) return;
    // Unify: -l implies -a, so include ALL when LONG is set
    if (flags & 1u) flags |= (1u<<1);
    g_list_flags = flags;
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* use = path && path[0] ? path : cwd;
    normalize_abs_path(use, cwd, absBuf, (int)sizeof(absBuf));
    // Root always lists root
    if (absBuf[0] == '/' && absBuf[1] == 0) { kos::fs::g_fs_ptr->ListRoot(); g_list_flags = 0; return; }
    // Optional safety: if DirExists says no for '/', still list root
    if (!kos::fs::g_fs_ptr->DirExists(absBuf)) {
    if (absBuf[0] == '/' && absBuf[1] == 0) { kos::fs::g_fs_ptr->ListRoot(); g_list_flags = 0; return; }
        TTY::Write((const int8_t*)"ls: path not found: ");
        TTY::Write(absBuf);
        TTY::PutChar('\n');
        g_list_flags = 0;
        return;
    }
    kos::fs::g_fs_ptr->ListDir(absBuf);
    g_list_flags = 0;
}

// Read a file into buffer; returns bytes read or -1
extern "C" int32_t sys_readfile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
    if (!kos::fs::g_fs_ptr || !outBuf || maxLen == 0) return -1;
    // Resolve relative path against current working directory and normalize
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* in = (path && path[0]) ? path : (const int8_t*)"/";
    normalize_abs_path(in, cwd, absBuf, (int)sizeof(absBuf));
    return kos::fs::g_fs_ptr->ReadFile(absBuf, outBuf, maxLen);
}

// Execute an ELF32 by path with argv; returns 0 on success
extern "C" int32_t sys_exec(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline) {
    if (!kos::fs::g_fs_ptr || !path) return -1;
    // Load file
    static uint8_t elfBuf[256*1024];
    int32_t n = kos::fs::g_fs_ptr->ReadFile(path, elfBuf, sizeof(elfBuf));
    if (n <= 0) {
        TTY::Write((const int8_t*)"exec: not found: "); TTY::Write(path); TTY::PutChar('\n');
        return -1;
    }
    // Pass args and cmdline into API table
    kos::sys::SetArgs(argc, argv, cmdline ? cmdline : path);
    if (!kos::lib::ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n)) return -1;
    return 0;
}
extern "C" int32_t sys_mkdir(const int8_t* path, int32_t parents) {
    if (kos::fs::g_fs_ptr) {
        // Resolve relative paths against current working directory and normalize (handle ., .., duplicate slashes)
        int8_t absBuf[160];
        const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
        const int8_t* in = path && path[0] ? path : (const int8_t*)"/";
        normalize_abs_path(in, cwd, absBuf, (int)sizeof(absBuf));
    return kos::fs::g_fs_ptr->Mkdir(absBuf, parents);
    }
    TTY::Write((const int8_t*)"mkdir: no filesystem mounted\n");
    return -1;
}

// Rename syscall: absolute, normalized, call into filesystem
extern "C" int32_t sys_rename(const int8_t* src, const int8_t* dst) {
    if (!kos::fs::g_fs_ptr || !src || !dst) return -1;
    int8_t absSrc[160], absDst[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    normalize_abs_path(src, cwd, absSrc, (int)sizeof(absSrc));
    normalize_abs_path(dst, cwd, absDst, (int)sizeof(absDst));
    return kos::fs::g_fs_ptr->Rename(absSrc, absDst);
}

// Socket enumeration bridging to lib::SocketEnumerate for current kernel sockets
extern "C" int32_t sys_net_list_sockets(void* out, int32_t max, int32_t want_tcp, int32_t want_udp, int32_t listening_only) {
    if (!out || max <= 0) return -1;
    kos_sockinfo_t* arr = reinterpret_cast<kos_sockinfo_t*>(out);
    kos::lib::SocketEnumEntry tmp[64];
    int m = (max < 64) ? max : 64;
    int n = kos::lib::SocketEnumerate(tmp, m);
    int w = 0;
    for (int i = 0; i < n; ++i) {
        const char* p = tmp[i].proto ? tmp[i].proto : "";
        int is_tcp = (p[0]=='t' && p[1]=='c' && p[2]=='p');
        int is_udp = (p[0]=='u' && p[1]=='d' && p[2]=='p');
        // Filter by proto
        if ((want_tcp || want_udp) && !((want_tcp && is_tcp) || (want_udp && is_udp))) continue;
        // Filter listening-only
        if (listening_only) {
            const char* st = tmp[i].state ? tmp[i].state : "";
            if (!(st[0]=='L' && st[1]=='I')) continue;
        }
        arr[w].proto = tmp[i].proto;
        arr[w].state = tmp[i].state;
        arr[w].laddr = tmp[i].laddr;
        arr[w].lport = (uint16_t)tmp[i].lport;
        arr[w].raddr = tmp[i].raddr;
        arr[w].rport = (uint16_t)tmp[i].rport;
        arr[w].pid   = tmp[i].pid;
        arr[w].prog  = tmp[i].prog;
        ++w;
        if (w >= max) break;
    }
    return w;
}

// Directory enumeration syscall - wraps filesystem EnumDir
// The callback signature differs between C app (returns int) and kernel (returns bool)
// We adapt it here

// Static data for enumdir callback adapter
struct EnumdirCallbackData {
    int32_t (*user_cb)(const void*, void*);
    void* user_data;
};

static bool enumdir_adapter(const kos::fs::DirEntry* entry, void* data) {
    EnumdirCallbackData* cd = reinterpret_cast<EnumdirCallbackData*>(data);
    // Call user callback with entry as void*, returns 0 to stop, non-zero to continue
    int32_t result = cd->user_cb(reinterpret_cast<const void*>(entry), cd->user_data);
    return result != 0;
}

extern "C" int32_t sys_enumdir(const int8_t* path, int32_t (*callback)(const void*, void*), void* userdata) {
    if (!kos::fs::g_fs_ptr || !callback) return -1;
    
    // Resolve path
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* use = path && path[0] ? path : cwd;
    normalize_abs_path(use, cwd, absBuf, (int)sizeof(absBuf));
    
    EnumdirCallbackData cbData = { callback, userdata };
    return kos::fs::g_fs_ptr->EnumDir(absBuf, enumdir_adapter, &cbData);
}

// --- Simple argument storage for current process/app ---
static int32_t g_argc = 0;
static const int8_t* g_argv_vec[16];
static int8_t g_argv_storage[512];

// Helpers: place cmdline and cwd buffers inside the API table page at 0x0007F000
// Layout: [ApiTable][cmdline 128][cwd 128]
static inline int8_t* api_cmdline_buf() {
    return (int8_t*)((uint32_t)kos::sys::table() + sizeof(kos::sys::ApiTable));
}
static inline int8_t* api_cwd_buf() {
    return api_cmdline_buf() + 128;
}

extern "C" int32_t sys_get_argc() { return g_argc; }
extern "C" const int8_t* sys_get_arg(int32_t index) {
    if (index < 0 || index >= g_argc) return nullptr;
    return g_argv_vec[index];
}

// Memory information system calls
extern "C" uint32_t sys_get_total_frames() {
    return kos::memory::PMM::TotalFrames();
}

extern "C" uint32_t sys_get_free_frames() {
    return kos::memory::PMM::FreeFrames();
}

extern "C" uint32_t sys_get_heap_size() {
    // Heap size is the difference between end and base (0x02000000)
    virt_addr_t end = kos::memory::Heap::End();
    const virt_addr_t base = 0x02000000;
    return (end >= base) ? (uint32_t)(end - base) : 0;
}

extern "C" uint32_t sys_get_heap_used() {
    return kos::memory::Heap::Used();
}

// Helper: normalize and make absolute path based on cwd
static void normalize_abs_path(const int8_t* inPath, const int8_t* cwd, int8_t* outBuf, int outSize) {
    if (!inPath || !outBuf || outSize <= 0) { if (outSize > 0) outBuf[0] = 0; return; }
    int8_t tmp[160];
    if (inPath[0] == '/') {
        // copy inPath into tmp
        int i = 0; for (; inPath[i] && i < (int)sizeof(tmp)-1; ++i) tmp[i] = inPath[i]; tmp[i] = 0;
    } else {
        // join cwd + '/' + inPath
        const int8_t* base = cwd ? cwd : (const int8_t*)"/";
        int i = 0; for (; base[i] && i < (int)sizeof(tmp)-1; ++i) tmp[i] = base[i];
        if (i == 0 || tmp[i-1] != '/') { if (i < (int)sizeof(tmp)-1) tmp[i++] = '/'; }
        int j = 0; while (inPath[j] && i < (int)sizeof(tmp)-1) tmp[i++] = inPath[j++];
        tmp[i] = 0;
    }

    // normalize tmp into outBuf
    outBuf[0] = '/'; int out = 1;
    int segStart[32]; int segCount = 0;
    const int8_t* s = tmp;
    while (*s) {
        while (*s == '/') ++s;
        if (!*s) break;
        int8_t seg[80]; int k = 0;
        while (*s && *s != '/' && k < (int)sizeof(seg)-1) { seg[k++] = *s++; }
        seg[k] = 0;
        if (k == 0) continue;
        if (seg[0] == '.' && seg[1] == 0) continue;
        if (seg[0] == '.' && seg[1] == '.' && seg[2] == 0) {
            if (segCount > 0) { out = segStart[--segCount]; }
            continue;
        }
        if (out > 1 && out < outSize-1 && outBuf[out-1] != '/') outBuf[out++] = '/';
        segStart[segCount++] = out;
        for (int t = 0; seg[t] && out < outSize-1; ++t) outBuf[out++] = seg[t];
    }
    if (out > 1 && outBuf[out-1] == '/') --out;
    outBuf[out] = 0;
}

extern "C" int32_t sys_chdir(const int8_t* path) {
    // Resolve path relative to current cwd
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    normalize_abs_path(path ? path : (const int8_t*)"/", cwd, absBuf, (int)sizeof(absBuf));
    // Validate directory exists using filesystem if available
    if (kos::fs::g_fs_ptr && !kos::fs::g_fs_ptr->DirExists(absBuf)) {
        // Do not print here; let the caller decide how to report errors
        return -1;
    }
    kos::sys::SetCwd(absBuf);
    return 0;
}

namespace kos { namespace sys {
    // Setter utilities for the kernel/shell to populate before launching an app
    void SetArgs(int32_t argc, const int8_t** argv, const int8_t* cmdline) {
        if (argc < 0) argc = 0;
        if (argc > (int32_t)(sizeof(g_argv_vec)/sizeof(g_argv_vec[0]))) argc = (int32_t)(sizeof(g_argv_vec)/sizeof(g_argv_vec[0]));
        
        // Copy cmdline into API page buffer so apps (even in user mode) can read it
        int8_t* dstCmd = api_cmdline_buf();
        int pos = 0;
        if (cmdline) {
            while (cmdline[pos] && pos < 127) { dstCmd[pos] = cmdline[pos]; ++pos; }
        }
        dstCmd[pos] = 0;
        table()->cmdline = dstCmd;

        // Copy argv strings into a contiguous storage and set pointers
        int storagePos = 0;
        g_argc = 0;
        for (int32_t i = 0; i < argc; ++i) {
            const int8_t* s = (argv ? argv[i] : nullptr);
            if (!s) { g_argv_vec[g_argc] = nullptr; ++g_argc; continue; }
            // copy token
            int start = storagePos;
            while (*s && storagePos < (int)sizeof(g_argv_storage) - 1) { g_argv_storage[storagePos++] = *s++; }
            if (storagePos >= (int)sizeof(g_argv_storage) - 1) { // out of space
                g_argv_storage[(int)sizeof(g_argv_storage) - 1] = 0;
                break;
            }
            g_argv_storage[storagePos++] = 0;
            g_argv_vec[g_argc] = &g_argv_storage[start];
            ++g_argc;
        }

        // Ensure table()->cmdline points to the API page buffer
        table()->cmdline = dstCmd;
    }

    void SetCwd(const int8_t* path) {
        // Store CWD string inside API page so user apps can read it safely
        int8_t* dst = api_cwd_buf();
        int i = 0;
        if (!path) { dst[0] = '/'; dst[1] = 0; }
        else {
            while (path[i] && i < 127) { dst[i] = path[i]; ++i; }
            dst[i] = 0;
        }
        table()->cwd = dst;
    }
    // Provide a lightweight accessor so fs code can check listing flags
    uint32_t CurrentListFlags() { return g_list_flags; }
}}

extern "C" void InitSysApi() {
    ApiTable* t = table();
    t->putc = &sys_putc;
    t->puts = &sys_puts;
    t->hex  = &sys_hex;
    t->listroot = &sys_listroot;
    t->listdir = &sys_listdir;
    t->listdir_ex = &sys_listdir_ex;
    t->clear = &sys_clear;
    // IMPORTANT: follow the exact struct field order declared in headers
    // After 'clear', expose color controls, then args and paths
    t->set_attr = &sys_set_attr;
    t->set_color = &sys_set_color;
    // Then: get_argc, get_arg, cmdline, mkdir, chdir, cwd
    t->get_argc = &sys_get_argc;
    t->get_arg  = &sys_get_arg;
    // Initialize cmdline and cwd buffers within API page
    int8_t* cmdBuf = api_cmdline_buf(); cmdBuf[0] = 0;
    int8_t* cwdBuf = api_cwd_buf(); cwdBuf[0] = '/'; cwdBuf[1] = 0; cwdBuf[2] = 0;
    t->cmdline  = cmdBuf;
    t->mkdir    = &sys_mkdir;
    t->chdir    = &sys_chdir;
    t->cwd      = cwdBuf;
    // Memory information functions
    t->get_total_frames = &sys_get_total_frames;
    t->get_free_frames = &sys_get_free_frames;
    t->get_heap_size = &sys_get_heap_size;
    t->get_heap_used = &sys_get_heap_used;
    // Hardware helpers
    t->pci_cfg_read = &sys_pci_cfg_read;
    // New APIs
    t->readfile = &sys_readfile;
    t->exec = &sys_exec;
    // Process info provider for tools like 'top'
    t->get_process_info = &sys_get_process_info;
    // App key polling
    t->key_poll = &sys_key_poll;
    // Date/time API
    t->get_datetime = &sys_get_datetime;
    // Battery API
    t->get_battery_percent = &sys_get_battery_percent;
    // File rename/move
    t->rename = &sys_rename;
    // Networking: socket enumeration (stub)
    t->net_list_sockets = &sys_net_list_sockets;
    // Directory enumeration
    t->enumdir = &sys_enumdir;
}