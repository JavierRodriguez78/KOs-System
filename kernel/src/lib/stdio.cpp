#include <lib/stdio.hpp>
// Forward declarations are in header; implementation lives in sysapi.cpp

using namespace kos::common;

namespace kos { namespace sys {

    static inline ApiTable* table_raw() {
        return (ApiTable*)0x0007F000;
    }

    ApiTable* table() { return table_raw(); }

    void putc(int8_t c) { if (table_raw()->putc) table_raw()->putc(c); }
    void puts(const int8_t* s) { if (table_raw()->puts) table_raw()->puts(s); }
    void hex(uint8_t v) { if (table_raw()->hex) table_raw()->hex(v); }
    void listroot() { if (table_raw()->listroot) table_raw()->listroot(); }
    void clear() { if (table_raw()->clear) table_raw()->clear(); }

    int32_t argc() { return (table_raw()->get_argc) ? table_raw()->get_argc() : 0; }
    const int8_t* argv(int32_t index) { return (table_raw()->get_arg) ? table_raw()->get_arg(index) : nullptr; }
    const int8_t* cmdline() { return table_raw()->cmdline; }

    static void print_uint(uint32_t v, uint32_t base, bool upper, int padWidth = 0, bool padZero = false) {
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

    void vprintf(const int8_t* fmt, va_list ap) {
        for (int i = 0; fmt && fmt[i]; ++i) {
            if (fmt[i] != '%') { putc(fmt[i]); continue; }
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
                    putc('%'); putc(spec);
                    break;
            }
        }
    }

    void printf(const int8_t* fmt, ...) {
        va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    }

}}