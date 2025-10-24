#include <lib/stdio.hpp>
#include <console/tty.hpp>
#include <fs/filesystem.hpp>
#include <lib/elfloader.hpp>
#include <memory/pmm.hpp>
#include <memory/heap.hpp>
#include <memory/paging.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_constants.hpp>

using namespace kos::sys;
using namespace kos::console;
using namespace kos::common;

extern kos::fs::Filesystem* g_fs_ptr;
static uint32_t g_list_flags = 0; // flags used by listdir_ex

// Forward declaration for path normalization used by sys_listdir and sys_chdir
static void normalize_abs_path(const int8_t* inPath, const int8_t* cwd, int8_t* outBuf, int outSize);

extern "C" void sys_putc(int8_t c) { TTY::PutChar(c); }
extern "C" void sys_puts(const int8_t* s) { TTY::Write(s); }
extern "C" void sys_hex(uint8_t v) { TTY::WriteHex(v); }
extern "C" void sys_listroot() { if (g_fs_ptr) g_fs_ptr->ListRoot(); }
extern "C" void sys_clear() { TTY::Clear(); }
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
extern "C" void sys_listdir(const int8_t* path) {
    if (!g_fs_ptr) return;
    // Resolve relative path against current working directory and normalize
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* use = path && path[0] ? path : cwd;
    normalize_abs_path(use, cwd, absBuf, (int)sizeof(absBuf));
    // Treat any form of root ("/" or only slashes) as ListRoot
    bool isRoot = (absBuf[0] == '/') && (absBuf[1] == 0);
    if (isRoot) { g_fs_ptr->ListRoot(); return; }
    // If the path doesn't exist as a directory, report a clear error up-front
    if (!g_fs_ptr->DirExists(absBuf)) {
        // As a safety net, if it somehow normalizes to root but DirExists disagrees, still list root
        if (absBuf[0] == '/' && absBuf[1] == 0) { g_fs_ptr->ListRoot(); return; }
        TTY::Write((const int8_t*)"ls: path not found: ");
        TTY::Write(absBuf);
        TTY::PutChar('\n');
        return;
    }
    g_fs_ptr->ListDir(absBuf);
}

extern "C" void sys_listdir_ex(const int8_t* path, uint32_t flags) {
    if (!g_fs_ptr) return;
    // Unify: -l implies -a, so include ALL when LONG is set
    if (flags & 1u) flags |= (1u<<1);
    g_list_flags = flags;
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* use = path && path[0] ? path : cwd;
    normalize_abs_path(use, cwd, absBuf, (int)sizeof(absBuf));
    // Root always lists root
    if (absBuf[0] == '/' && absBuf[1] == 0) { g_fs_ptr->ListRoot(); g_list_flags = 0; return; }
    // Optional safety: if DirExists says no for '/', still list root
    if (!g_fs_ptr->DirExists(absBuf)) {
        if (absBuf[0] == '/' && absBuf[1] == 0) { g_fs_ptr->ListRoot(); g_list_flags = 0; return; }
        TTY::Write((const int8_t*)"ls: path not found: ");
        TTY::Write(absBuf);
        TTY::PutChar('\n');
        g_list_flags = 0;
        return;
    }
    g_fs_ptr->ListDir(absBuf);
    g_list_flags = 0;
}

// Read a file into buffer; returns bytes read or -1
extern "C" int32_t sys_readfile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
    if (!g_fs_ptr || !outBuf || maxLen == 0) return -1;
    // Resolve relative path against current working directory and normalize
    int8_t absBuf[160];
    const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
    const int8_t* in = (path && path[0]) ? path : (const int8_t*)"/";
    normalize_abs_path(in, cwd, absBuf, (int)sizeof(absBuf));
    return g_fs_ptr->ReadFile(absBuf, outBuf, maxLen);
}

// Execute an ELF32 by path with argv; returns 0 on success
extern "C" int32_t sys_exec(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline) {
    if (!g_fs_ptr || !path) return -1;
    // Load file
    static uint8_t elfBuf[256*1024];
    int32_t n = g_fs_ptr->ReadFile(path, elfBuf, sizeof(elfBuf));
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
    if (g_fs_ptr) {
        // Resolve relative paths against current working directory and normalize (handle ., .., duplicate slashes)
        int8_t absBuf[160];
        const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
        const int8_t* in = path && path[0] ? path : (const int8_t*)"/";
        normalize_abs_path(in, cwd, absBuf, (int)sizeof(absBuf));
        return g_fs_ptr->Mkdir(absBuf, parents);
    }
    TTY::Write((const int8_t*)"mkdir: no filesystem mounted\n");
    return -1;
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
    if (g_fs_ptr && !g_fs_ptr->DirExists(absBuf)) {
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
    // After 'clear', the order is: get_argc, get_arg, cmdline, mkdir, chdir, cwd
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
}