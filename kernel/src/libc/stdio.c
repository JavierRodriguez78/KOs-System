#include <lib/libc/stdio.h>
#include <stdarg.h>

// Minimal snprintf implementation for KOS
int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int written = 0;
    const char *f = format;
    char *out = str;
    size_t left = size ? size - 1 : 0;
    while (*f && left > 0) {
        if (*f == '%') {
            ++f;
            if (*f == 's') {
                const char *s = va_arg(args, const char *);
                while (*s && left > 0) { *out++ = *s++; --left; ++written; }
            } else if (*f == 'd' || *f == 'u') {
                int v = va_arg(args, int);
                char buf[16];
                int len = 0;
                if (*f == 'd' && v < 0) { *out++ = '-'; v = -v; --left; ++written; }
                do { buf[len++] = '0' + (v % 10); v /= 10; } while (v && len < 15);
                for (int i = len-1; i >= 0 && left > 0; --i) { *out++ = buf[i]; --left; ++written; }
            } else if (*f == 'x' || *f == 'X') {
                unsigned int v = va_arg(args, unsigned int);
                char buf[16];
                int len = 0;
                do { buf[len++] = "0123456789abcdef"[v % 16]; v /= 16; } while (v && len < 15);
                for (int i = len-1; i >= 0 && left > 0; --i) { *out++ = buf[i]; --left; ++written; }
            } else if (*f == 'c') {
                char c = (char)va_arg(args, int);
                *out++ = c; --left; ++written;
            } else {
                *out++ = '%'; --left; ++written;
                if (left > 0) { *out++ = *f; --left; ++written; }
            }
            ++f;
        } else {
            *out++ = *f++; --left; ++written;
        }
    }
    if (size) *out = '\0';
    va_end(args);
    return written;
}
