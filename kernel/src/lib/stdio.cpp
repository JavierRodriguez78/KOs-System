#include <lib/stdio.hpp>
// Forward declarations are in header; implementation lives in sysapi.cpp

using namespace kos::common;

namespace kos { 
    namespace sys {

        // Return raw pointer to system API table at fixed address
        static inline ApiTable* table_raw() {
            return (ApiTable*)0x0007F000;
        }

        // Return pointer to system API table
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
            if (v == 0) { 
                buf[i++] = '0'; 
            }
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

        // Minimal snprintf implementation using vprintf logic
        int snprintf(char *str, size_t size, const char *format, ...) {
            if (!str || size == 0 || !format) return -1;
            va_list ap;
            va_start(ap, format);
            size_t written = 0;
            for (size_t i = 0; format[i] && written + 1 < size; ++i) {
                if (format[i] != '%') {
                    str[written++] = format[i];
                    continue;
                }
                // Handle format specifier (only %s, %d, %u, %x, %c, %% for now)
                ++i;
                char spec = format[i];
                char buf[32];
                switch (spec) {
                    case '%': str[written++] = '%'; break;
                    case 'c': {
                        int c = va_arg(ap, int);
                        str[written++] = (char)c;
                        break;
                    }
                    case 's': {
                        const char* s = va_arg(ap, const char*);
                        if (!s) s = "(null)";
                        for (size_t j = 0; s[j] && written + 1 < size; ++j)
                            str[written++] = s[j];
                        break;
                    }
                    case 'd': case 'i': {
                        int v = va_arg(ap, int);
                        bool neg = v < 0;
                        unsigned int uv = neg ? -v : v;
                        int len = 0;
                        if (neg) buf[len++] = '-';
                        do { buf[len++] = '0' + (uv % 10); uv /= 10; } while (uv && len < 31);
                        for (int k = len - 1; k >= 0 && written + 1 < size; --k)
                            str[written++] = buf[k];
                        break;
                    }
                    case 'u': {
                        unsigned int v = va_arg(ap, unsigned int);
                        int len = 0;
                        do { buf[len++] = '0' + (v % 10); v /= 10; } while (v && len < 31);
                        for (int k = len - 1; k >= 0 && written + 1 < size; --k)
                            str[written++] = buf[k];
                        break;
                    }
                    case 'x': case 'X': {
                        unsigned int v = va_arg(ap, unsigned int);
                        int len = 0;
                        const char* digs = (spec == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                        do { buf[len++] = digs[v % 16]; v /= 16; } while (v && len < 31);
                        for (int k = len - 1; k >= 0 && written + 1 < size; --k)
                            str[written++] = buf[k];
                        break;
                    }
                    default:
                        str[written++] = '%';
                        str[written++] = spec;
                        break;
                    }
                }
            va_end(ap);
            str[written] = '\0';
            return (int)written;
        }

    // --- Minimal keyboard input consumer for scanf ---
    // Simple single-consumer buffer to accumulate keystrokes offered
    // by the keyboard handler via TryDeliverKey. We avoid dynamic
    // allocation and keep it tiny.
    static volatile bool g_input_active = false; // scanf is waiting
    static volatile int g_buf_head = 0;
    static volatile int g_buf_tail = 0;
    static int8_t g_input_buf[256];

    // Offer a character from keyboard path; returns true if consumed.
    bool TryDeliverKey(int8_t c) {
        if (!g_input_active) return false;
        // Handle backspace locally: remove last buffered char if any
        if (c == '\b' || c == 127) {
            if (g_buf_head != g_buf_tail) {
                // Decrement tail (mod 256)
                g_buf_tail = (g_buf_tail - 1) & 0xFF;
                // Erase on screen
                puts((const int8_t*)"\b \b");
            }
            return true; // consumed
        }
        if (c == '\r') c = '\n';
        int next_tail = (g_buf_tail + 1) & 0xFF;
        if (next_tail == g_buf_head) {
            // Buffer full; drop key so shell can handle (unlikely)
            return false;
        }
        g_input_buf[g_buf_tail] = c;
        g_buf_tail = next_tail;
        // Echo printable and newline
        if ((c >= 32 && c <= 126) || c == '\n') putc(c);
        return true;
    }

    // Blocking-like get next char for scanf; integrates with shell echoes
    static int getch_blocking() {
        // Wait until a char is available. If threading exists, yield.
        while (g_buf_head == g_buf_tail) {
            // Busy-wait fallback; if scheduler API is available, try to yield lightly
            // We cannot include scheduler headers here; keep it tight.
        }
        int8_t c = g_input_buf[g_buf_head];
        g_buf_head = (g_buf_head + 1) & 0xFF;
        return (int)(uint8_t)c;
    }

    // Helper: peek without consuming
    static int peekch() {
        if (g_buf_head == g_buf_tail) return -1;
        return (int)(uint8_t)g_input_buf[g_buf_head];
    }

    // Helper: consume one char if available
    static int popch() {
        if (g_buf_head == g_buf_tail) return -1;
        int8_t c = g_input_buf[g_buf_head];
        g_buf_head = (g_buf_head + 1) & 0xFF;
        return (int)(uint8_t)c;
    }

    // Basic scanners
    static void skip_ws() {
        int ch;
        while ((ch = peekch()) != -1) {
            if (ch==' ' || ch=='\t' || ch=='\n' || ch=='\r' || ch=='\v' || ch=='\f') {
                popch();
            } else break;
        }
    }

    static bool scan_int_signed(int32_t* out) {
        skip_ws();
        int ch = peekch();
        int sign = 1; int32_t val = 0; bool any=false;
        if (ch == '+' || ch == '-') { sign = (ch=='-') ? -1 : 1; popch(); }
        while ((ch = peekch()) != -1) {
            if (ch >= '0' && ch <= '9') { val = val*10 + (ch - '0'); any=true; popch(); }
            else break;
        }
        if (!any) return false;
        *out = val * sign;
        return true;
    }

    static bool scan_uint(uint32_t* out) {
        skip_ws();
        int ch; uint32_t val=0; bool any=false;
        while ((ch = peekch()) != -1) {
            if (ch >= '0' && ch <= '9') { val = val*10 + (ch - '0'); any=true; popch(); }
            else break;
        }
        if (!any) return false;
        *out = val; return true;
    }

    static uint8_t hexval(int ch) {
        if (ch >= '0' && ch <= '9') return (uint8_t)(ch - '0');
        if (ch >= 'a' && ch <= 'f') return (uint8_t)(10 + (ch - 'a'));
        if (ch >= 'A' && ch <= 'F') return (uint8_t)(10 + (ch - 'A'));
        return 0xFF;
    }

    static bool scan_hex(uint32_t* out) {
        skip_ws();
        int ch = peekch();
        // Optional 0x/0X
        if (ch == '0') {
            popch();
            int ch2 = peekch();
            if (ch2 == 'x' || ch2 == 'X') { popch(); }
            else { // it was just 0
                *out = 0; return true;
            }
        }
        uint32_t val=0; bool any=false;
        while ((ch = peekch()) != -1) {
            uint8_t v = hexval(ch);
            if (v != 0xFF) { val = (val<<4) | v; any=true; popch(); }
            else break;
        }
        if (!any) return false;
        *out = val; return true;
    }

    static bool scan_str(int8_t* dst, int maxLen) {
        if (!dst || maxLen <= 0) return false;
        skip_ws();
        int ch; int n=0; bool any=false;
        while ((ch = peekch()) != -1) {
            if (ch==' '||ch=='\t'||ch=='\n'||ch=='\r'||ch=='\v'||ch=='\f') break;
            if (n < maxLen-1) { dst[n++] = (int8_t)ch; any=true; }
            popch();
        }
        if (n < maxLen) dst[n] = 0; else dst[maxLen-1]=0;
        return any;
    }

    static bool scan_char(int8_t* out) {
        int ch = getch_blocking();
        if (ch < 0) return false;
        *out = (int8_t)ch; return true;
    }

    int scanf(const int8_t* fmt, ...) {
        if (!fmt) return 0;
        // Reset buffer pointers and mark active
        g_buf_head = g_buf_tail = 0; g_input_active = true;

        int assigned = 0;
        va_list ap; va_start(ap, fmt);
        for (int i=0; fmt[i]; ++i) {
            int8_t f = fmt[i];
            if (f == ' ' || f=='\t' || f=='\n') {
                // skip whitespace in input
                skip_ws();
                continue;
            }
            if (f != '%') {
                // Literal match: consume and require it
                int ch = getch_blocking();
                if (ch != f) { break; }
                continue;
            }
            // Format specifier
            ++i; char spec = fmt[i];
            if (!spec) break;
            switch (spec) {
                case 'd': {
                    int32_t* p = va_arg(ap, int32_t*);
                    int32_t v;
                    if (scan_int_signed(&v)) { if (p) *p = v; assigned++; }
                    else { goto done; }
                } break;
                case 'u': {
                    uint32_t* p = va_arg(ap, uint32_t*);
                    uint32_t v;
                    if (scan_uint(&v)) { if (p) *p = v; assigned++; }
                    else { goto done; }
                } break;
                case 'x': case 'X': {
                    uint32_t* p = va_arg(ap, uint32_t*);
                    uint32_t v;
                    if (scan_hex(&v)) { if (p) *p = v; assigned++; }
                    else { goto done; }
                } break;
                case 's': {
                    int8_t* dst = va_arg(ap, int8_t*);
                    // No width parsing for simplicity; assume caller provided enough space
                    if (scan_str(dst, 128)) { assigned++; }
                    else { goto done; }
                } break;
                case 'c': {
                    int8_t* p = va_arg(ap, int8_t*);
                    int8_t ch;
                    if (scan_char(&ch)) { if (p) *p = ch; assigned++; }
                    else { goto done; }
                } break;
                case '%': {
                    int ch = getch_blocking();
                    if (ch != '%') { goto done; }
                } break;
                default:
                    // Unsupported spec - treat as literal
                    {
                        int ch = getch_blocking();
                        if (ch != spec) { goto done; }
                    }
                    break;
            }
        }
    done:
        va_end(ap);
        g_input_active = false;
        return assigned;
    }

}}