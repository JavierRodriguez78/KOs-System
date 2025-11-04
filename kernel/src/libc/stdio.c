#include <lib/libc/stdio.h>
#include <stdarg.h>

// Minimal vsnprintf/snprintf implementation for KOS
int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    int written = 0;
    const char *f = format;
    char *out = str;
    size_t left = size ? size - 1 : 0;
    while (*f && left > 0) {
        if (*f == '%') {
            ++f;
            if (*f == 's') {
                const char *s = va_arg(args, const char *);
                while (s && *s && left > 0) { *out++ = *s++; --left; ++written; }
            } else if (*f == 'd' || *f == 'i' || *f == 'u') {
                unsigned int uv;
                int v = va_arg(args, int);
                if (*f == 'd' || *f == 'i') {
                    if (v < 0) { if (left > 0) { *out++ = '-'; --left; ++written; } uv = (unsigned int)(-v); }
                    else uv = (unsigned int)v;
                } else {
                    uv = (unsigned int)v;
                }
                char buf[16];
                int len = 0;
                do { buf[len++] = '0' + (uv % 10u); uv /= 10u; } while (uv && len < 15);
                for (int i = len-1; i >= 0 && left > 0; --i) { *out++ = buf[i]; --left; ++written; }
            } else if (*f == 'x' || *f == 'X') {
                unsigned int v = va_arg(args, unsigned int);
                const char* digs = (*f == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                char buf[16];
                int len = 0;
                do { buf[len++] = digs[v & 0xF]; v >>= 4; } while (v && len < 15);
                for (int i = len-1; i >= 0 && left > 0; --i) { *out++ = buf[i]; --left; ++written; }
            } else if (*f == 'c') {
                char c = (char)va_arg(args, int);
                if (left > 0) { *out++ = c; --left; ++written; }
            } else if (*f == '%') {
                if (left > 0) { *out++ = '%'; --left; ++written; }
            } else {
                // Unknown specifier: print as literal
                if (left > 0) { *out++ = '%'; --left; ++written; }
                if (left > 0) { *out++ = *f; --left; ++written; }
            }
            ++f;
        } else {
            *out++ = *f++; --left; ++written;
        }
    }
    if (size) *out = '\0';
    return written;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}
