#include <lib/stdio.hpp>
#include <console/tty.hpp>
#include <fs/filesystem.hpp>

using namespace kos::sys;
using namespace kos::console;
using namespace kos::common;

extern kos::fs::Filesystem* g_fs_ptr;

extern "C" void sys_putc(int8_t c) { TTY::PutChar(c); }
extern "C" void sys_puts(const int8_t* s) { TTY::Write(s); }
extern "C" void sys_hex(uint8_t v) { TTY::WriteHex(v); }
extern "C" void sys_listroot() { if (g_fs_ptr) g_fs_ptr->ListRoot(); }
extern "C" void sys_listdir(const int8_t* path) {
    if (!g_fs_ptr || !path) return;
    // Minimal listing via Filesystem interface: if FAT32 mounted, reuse helper; fallback to root list
    // For now, just attempt to read entries from the target directory cluster by using GetCommandEntry/Exists or direct prints.
    // As a simple implementation here, call ListRoot when path=="/"; otherwise print a hint.
    if (path[0] == '/' && path[1] == 0) { g_fs_ptr->ListRoot(); return; }
    TTY::Write((const int8_t*)"ls: listing of arbitrary dirs not yet implemented; showing root instead\n");
    g_fs_ptr->ListRoot();
}
extern "C" int32_t sys_mkdir(const int8_t* path, int32_t parents) {
    if (g_fs_ptr) {
        // Resolve relative paths against current working directory
        const int8_t* usePath = path;
        int8_t absBuf[160];
        if (path && path[0] != '/') {
            const int8_t* cwd = table()->cwd ? table()->cwd : (const int8_t*)"/";
            // Build abs: cwd + '/' + path, avoiding double '/'
            int i = 0;
            for (; cwd[i] && i < (int)sizeof(absBuf)-1; ++i) absBuf[i] = cwd[i];
            if (i == 0 || absBuf[i-1] != '/') { if (i < (int)sizeof(absBuf)-1) absBuf[i++] = '/'; }
            int j = 0; while (path[j] && i < (int)sizeof(absBuf)-1) absBuf[i++] = path[j++];
            absBuf[i] = 0;
            usePath = absBuf;
        }
        return g_fs_ptr->Mkdir(usePath, parents);
    }
    TTY::Write((const int8_t*)"mkdir: no filesystem mounted\n");
    return -1;
}

// --- Simple argument storage for current process/app ---
static int32_t g_argc = 0;
static const int8_t* g_argv_vec[16];
static const int8_t* g_cmdline = nullptr;
static int8_t g_cmdline_buf[128];
static int8_t g_argv_storage[512];
static int8_t g_cwd_buf[128] = "/";

extern "C" int32_t sys_get_argc() { return g_argc; }
extern "C" const int8_t* sys_get_arg(int32_t index) {
    if (index < 0 || index >= g_argc) return nullptr;
    return g_argv_vec[index];
}

namespace kos { namespace sys {
    // Setter utilities for the kernel/shell to populate before launching an app
    void SetArgs(int32_t argc, const int8_t** argv, const int8_t* cmdline) {
        if (argc < 0) argc = 0;
        if (argc > (int32_t)(sizeof(g_argv_vec)/sizeof(g_argv_vec[0]))) argc = (int32_t)(sizeof(g_argv_vec)/sizeof(g_argv_vec[0]));

        // Copy cmdline to internal buffer
        int pos = 0;
        if (cmdline) {
            while (cmdline[pos] && pos < (int)sizeof(g_cmdline_buf) - 1) { g_cmdline_buf[pos] = cmdline[pos]; ++pos; }
        }
        g_cmdline_buf[pos] = 0;
        g_cmdline = g_cmdline_buf;

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

        // Update API table pointer so apps can read cmdline
        table()->cmdline = g_cmdline;
    }

    void SetCwd(const int8_t* path) {
        int i = 0;
        if (!path) { g_cwd_buf[0] = '/'; g_cwd_buf[1] = 0; }
        else {
            while (path[i] && i < (int)sizeof(g_cwd_buf)-1) { g_cwd_buf[i] = path[i]; ++i; }
            g_cwd_buf[i] = 0;
        }
        table()->cwd = g_cwd_buf;
    }
}}

extern "C" void InitSysApi() {
    ApiTable* t = table();
    t->putc = &sys_putc;
    t->puts = &sys_puts;
    t->hex  = &sys_hex;
    t->listroot = &sys_listroot;
    t->listdir = &sys_listdir;
    t->mkdir = &sys_mkdir;
    t->get_argc = &sys_get_argc;
    t->get_arg = &sys_get_arg;
    t->cmdline = g_cmdline;
    t->cwd = g_cwd_buf;
}