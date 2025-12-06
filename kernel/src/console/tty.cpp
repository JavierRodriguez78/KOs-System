#include <console/tty.hpp>
#include <drivers/vga/vga.hpp>
#ifndef KOS_BUILD_APPS
#include <graphics/terminal.hpp>
#endif

using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers::vga;

    
    VGA TTY::vga;

#ifndef KOS_BUILD_APPS
// Pre-initialization buffer to capture shell output before graphical terminal exists
static char g_preterm_buffer[4096];
static uint32_t g_preterm_len = 0;
static bool g_preterm_flushed = false;

static void preterm_store(const int8_t* s) {
    if (g_preterm_flushed) return;
    if (!s) return;
    while (*s && g_preterm_len < sizeof(g_preterm_buffer)-1) {
        g_preterm_buffer[g_preterm_len++] = (char)*s++;
    }
    g_preterm_buffer[g_preterm_len] = 0;
}

static void preterm_store_char(int8_t c) {
    if (g_preterm_flushed) return;
    if (g_preterm_len < sizeof(g_preterm_buffer)-1) {
        g_preterm_buffer[g_preterm_len++] = (char)c;
        g_preterm_buffer[g_preterm_len] = 0;
    }
}

static void preterm_try_flush() {
    if (g_preterm_flushed) return;
    if (!kos::gfx::Terminal::IsActive()) return;
    if (g_preterm_len) {
        kos::gfx::Terminal::Write((const int8_t*)g_preterm_buffer);
    }
    g_preterm_flushed = true; // discard subsequent buffering
}
#endif

    void TTY::Clear(){
        vga.Clear();
        #ifndef KOS_BUILD_APPS
        preterm_store((const int8_t*)"\f"); // marker
        kos::gfx::Terminal::Clear();
        #endif
    }

    void TTY::PutChar(int8_t c){
        vga.PutChar(c);
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            preterm_try_flush();
            kos::gfx::Terminal::PutChar(c);
        } else {
            preterm_store_char(c);
        }
        #endif
    }

    void TTY::Write(const int8_t* str){
        vga.Write(str);
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            preterm_try_flush();
            kos::gfx::Terminal::Write(str);
        } else {
            preterm_store(str);
        }
        #endif
    }

    void TTY::WriteHex(uint8_t key){
        vga.WriteHex(key);
        // Hex output converts to two characters; we mimic with simple hex digits
        const char* hex = "0123456789ABCDEF";
        char buf[3]; buf[0] = hex[(key >> 4) & 0xF]; buf[1] = hex[key & 0xF]; buf[2] = 0;
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            preterm_try_flush();
            kos::gfx::Terminal::Write((const int8_t*)buf);
        } else {
            preterm_store((const int8_t*)buf);
        }
        #endif
    }

    void TTY::SetColor(uint8_t fg, uint8_t bg){
        vga.SetColor(fg, bg);
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            kos::gfx::Terminal::SetColor(fg, bg);
        }
        #endif
    }

    void TTY::SetAttr(uint8_t a){
        vga.SetAttr(a);
        // Propagate attribute to graphical terminal when active
        // VGA text mode attribute format: [BG(3):I][FG(3):I]
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            uint8_t fg = (a & 0x0F);
            uint8_t bg = ((a >> 4) & 0x0F);
            kos::gfx::Terminal::SetColor(fg, bg);
        }
        #endif
    }

    void TTY::MoveCursor(uint32_t col, uint32_t row) {
        vga.SetCursor((uint8_t)col, (uint8_t)row);
        #ifndef KOS_BUILD_APPS
        if (kos::gfx::Terminal::IsActive()) {
            kos::gfx::Terminal::MoveCursor(col, row);
        }
        #endif
    }

#ifndef KOS_BUILD_APPS
    void TTY::DiscardPreinitBuffer() {
        // Permanently drop any captured boot output so GUI terminal starts clean
        g_preterm_len = 0;
        g_preterm_buffer[0] = 0;
        g_preterm_flushed = true; // mark as flushed so it won't be printed later
    }
#else
    void TTY::DiscardPreinitBuffer() { /* no-op in apps build */ }
#endif