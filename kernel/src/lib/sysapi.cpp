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

extern "C" void InitSysApi() {
    ApiTable* t = table();
    t->putc = &sys_putc;
    t->puts = &sys_puts;
    t->hex  = &sys_hex;
    t->listroot = &sys_listroot;
}