#ifndef KOS_LIBC_STDIO_H
#define KOS_LIBC_STDIO_H

#include <lib/libc/stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ApiTableC {
    void (*putc)(int8_t c);
    void (*puts)(const int8_t* s);
    void (*hex)(uint8_t v);
    void (*listroot)();
    void (*listdir)(const int8_t* path);
    int32_t (*get_argc)();
    const int8_t* (*get_arg)(int32_t index);
    const int8_t* cmdline;
    int32_t (*mkdir)(const int8_t* path, int32_t parents);
    // Change current working directory. Returns 0 on success, negative on failure.
    int32_t (*chdir)(const int8_t* path);
    const int8_t* cwd;
} ApiTableC;

static inline ApiTableC* kos_sys_table(void) {
    return (ApiTableC*)0x0007F000; // fixed address set by kernel
}

static inline void kos_putc(int8_t c) { if (kos_sys_table()->putc) kos_sys_table()->putc(c); }
static inline void kos_puts(const int8_t* s) { if (kos_sys_table()->puts) kos_sys_table()->puts(s); }
static inline void kos_hex(uint8_t v) { if (kos_sys_table()->hex) kos_sys_table()->hex(v); }
static inline void kos_listroot(void) { if (kos_sys_table()->listroot) kos_sys_table()->listroot(); }
static inline void kos_listdir(const int8_t* path) { if (kos_sys_table()->listdir) kos_sys_table()->listdir(path); }

static inline int32_t kos_mkdir(const int8_t* path, int32_t parents) {
    if (kos_sys_table()->mkdir) return kos_sys_table()->mkdir(path, parents);
    return -1;
}

static inline int32_t kos_chdir(const int8_t* path) {
    if (kos_sys_table()->chdir) return kos_sys_table()->chdir(path);
    return -1;
}

static inline int32_t kos_argc(void) { return kos_sys_table()->get_argc ? kos_sys_table()->get_argc() : 0; }
static inline const int8_t* kos_argv(int32_t index) { return kos_sys_table()->get_arg ? kos_sys_table()->get_arg(index) : (const int8_t*)0; }
static inline const int8_t* kos_cmdline(void) { return kos_sys_table()->cmdline; }
static inline const int8_t* kos_cwd(void) { return kos_sys_table()->cwd; }

static inline void kos__print_uint32(unsigned int v, unsigned int base, int upper, int width, int padZero) {
    char buf[32];
    const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else {
        while (v && i < (int)sizeof(buf)) { buf[i++] = digs[v % base]; v /= base; }
    }
    int total = i;
    for (; total < width; ++total) kos_putc(padZero ? '0' : ' ');
    while (i--) kos_putc(buf[i]);
}

static inline void kos_vprintf(const int8_t* fmt, va_list ap) {
    for (int i = 0; fmt && fmt[i]; ++i) {
        if (fmt[i] != '%') { kos_putc(fmt[i]); continue; }
        int padZero = 0, width = 0; ++i;
        if (fmt[i] == '0') { padZero = 1; ++i; }
        while (fmt[i] >= '0' && fmt[i] <= '9') { width = width*10 + (fmt[i]-'0'); ++i; }
        char spec = fmt[i];
        switch (spec) {
            case '%': kos_putc('%'); break;
            case 'c': kos_putc((int8_t)va_arg(ap, int)); break;
            case 's': {
                const int8_t* s = (const int8_t*)va_arg(ap, const char*);
                kos_puts(s ? s : (const int8_t*)"(null)");
                break;
            }
            case 'd': case 'i': {
                int v = va_arg(ap, int);
                if (v < 0) { kos_putc('-'); kos__print_uint32((unsigned int)(-v), 10, 0, width, padZero); }
                else kos__print_uint32((unsigned int)v, 10, 0, width, padZero);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(ap, unsigned int);
                kos__print_uint32(v, 10, 0, width, padZero);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(ap, unsigned int);
                kos__print_uint32(v, 16, 0, width, padZero);
                break;
            }
            case 'X': {
                unsigned int v = va_arg(ap, unsigned int);
                kos__print_uint32(v, 16, 1, width, padZero);
                break;
            }
            default:
                kos_putc('%'); kos_putc(spec);
                break;
        }
    }
}

static inline void kos_printf(const int8_t* fmt, ...) {
    va_list ap; va_start(ap, fmt); kos_vprintf(fmt, ap); va_end(ap);
}

#ifdef __cplusplus
}
#endif

#endif