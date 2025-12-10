
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
    // Heap used is the difference between brk and base (0x02000000)
    virt_addr_t brk = kos::memory::Heap::Brk();
    const virt_addr_t base = 0x02000000;
    return (brk >= base) ? (uint32_t)(brk - base) : 0;
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
    // File rename/move
    t->rename = &sys_rename;
    // Networking: socket enumeration (stub)
    t->net_list_sockets = &sys_net_list_sockets;
}