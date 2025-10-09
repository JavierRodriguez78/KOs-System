// Minimal system API for apps to print to the shell/TTY.
// Kernel fills a function table at a fixed address.
#ifndef KOS_SYS_API_HPP
#define KOS_SYS_API_HPP

#include <common/types.hpp>
#include <stdarg.h>

namespace kos { namespace sys {

    struct ApiTable {
        void (*putc)(kos::common::int8_t c);
        void (*puts)(const kos::common::int8_t* s);
        void (*hex)(kos::common::uint8_t v);
        // List directory contents (currently lists filesystem root)
        void (*listroot)();
    };

    // Fixed address where kernel places the table
    static inline ApiTable* table() {
        return (ApiTable*)0x0007F000; // 4 KB below 0x80000 (adjust if needed)
    }

    // Convenience inline wrappers for apps
    static inline void putc(kos::common::int8_t c) { if (table()->putc) table()->putc(c); }
    static inline void puts(const kos::common::int8_t* s) { if (table()->puts) table()->puts(s); }
    static inline void hex(kos::common::uint8_t v) { if (table()->hex) table()->hex(v); }
    static inline void listroot() { if (table()->listroot) table()->listroot(); }

    // Minimal printf-style formatting: supports %s %c %d %i %u %x %X %p %%
    static inline void print_uint(kos::common::uint32_t v, kos::common::uint32_t base, bool upper, int padWidth = 0, bool padZero = false) {
        char buf[32];
        const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        int i = 0;
        if (v == 0) { buf[i++] = '0'; }
        else {
            while (v && i < (int)sizeof(buf)) { buf[i++] = digs[v % base]; v /= base; }
        }
        int total = i;
        for (; total < padWidth; ++total) putc(padZero ? '0' : ' ');
        while (i--) putc(buf[i]);
    }

    static inline void vprintf(const kos::common::int8_t* fmt, va_list ap) {
        using namespace kos::common;
        for (int i = 0; fmt && fmt[i]; ++i) {
            if (fmt[i] != '%') { putc(fmt[i]); continue; }
            // parse flags/width (very minimal: 0 and width digits)
            bool padZero = false; int width = 0; ++i;
            if (fmt[i] == '0') { padZero = true; ++i; }
            while (fmt[i] >= '0' && fmt[i] <= '9') { width = width*10 + (fmt[i]-'0'); ++i; }
            char spec = fmt[i];
            switch (spec) {
                case '%': putc('%'); break;
                case 'c': putc((int8_t)va_arg(ap, int)); break;
                case 's': {
                    const int8_t* s = (const int8_t*)va_arg(ap, const char*);
                    puts(s ? s : (const int8_t*)"(null)");
                    break;
                }
                case 'd': case 'i': {
                    int v = va_arg(ap, int);
                    if (v < 0) { putc('-'); print_uint((uint32_t)(-v), 10, false, width, padZero); }
                    else print_uint((uint32_t)v, 10, false, width, padZero);
                    break;
                }
                case 'u': {
                    uint32_t v = va_arg(ap, uint32_t);
                    print_uint(v, 10, false, width, padZero);
                    break;
                }
                case 'x': {
                    uint32_t v = va_arg(ap, uint32_t);
                    print_uint(v, 16, false, width, padZero);
                    break;
                }
                case 'X': {
                    uint32_t v = va_arg(ap, uint32_t);
                    print_uint(v, 16, true, width, padZero);
                    break;
                }
                case 'p': {
                    uint32_t v = (uint32_t)va_arg(ap, void*);
                    puts((const int8_t*)"0x");
                    print_uint(v, 16, false, sizeof(void*)*2, true);
                    break;
                }
                default:
                    // Unknown specifier, print literally
                    putc('%'); putc(spec);
                    break;
            }
        }
    }

    static inline void printf(const kos::common::int8_t* fmt, ...) {
        va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    }

}}

#endif