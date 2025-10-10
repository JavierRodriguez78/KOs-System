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

// --- Simple argument storage for current process/app ---
static int32_t g_argc = 0;
static const int8_t* g_argv_vec[16];
static const int8_t* g_cmdline = nullptr;
static int8_t g_cmdline_buf[128];
static int8_t g_argv_storage[512];

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
}}

extern "C" void InitSysApi() {
    ApiTable* t = table();
    t->putc = &sys_putc;
    t->puts = &sys_puts;
    t->hex  = &sys_hex;
    t->listroot = &sys_listroot;
    t->get_argc = &sys_get_argc;
    t->get_arg = &sys_get_arg;
    t->cmdline = g_cmdline;
}