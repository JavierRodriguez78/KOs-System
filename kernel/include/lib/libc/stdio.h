#ifndef KOS_LIBC_STDIO_H
#define KOS_LIBC_STDIO_H

#include <lib/libc/stdint.h>
#include <stdarg.h>
#include <lib/libc/stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ApiTableC {
    void (*putc)(int8_t c);
    void (*puts)(const int8_t* s);
    void (*hex)(uint8_t v);
    void (*listroot)();
    void (*listdir)(const int8_t* path);
    // Extended listdir with flags (bitmask). See KOS_LS_FLAG_* below.
    void (*listdir_ex)(const int8_t* path, uint32_t flags);
    // Clear text screen
    void (*clear)();
    int32_t (*get_argc)();
    const int8_t* (*get_arg)(int32_t index);
    const int8_t* cmdline;
    int32_t (*mkdir)(const int8_t* path, int32_t parents);
    // Change current working directory. Returns 0 on success, negative on failure.
    int32_t (*chdir)(const int8_t* path);
    const int8_t* cwd;
    // Memory information functions
    uint32_t (*get_total_frames)();
    uint32_t (*get_free_frames)();
    uint32_t (*get_heap_size)();
    uint32_t (*get_heap_used)();
    // PCI config space read helper (kernel mediates privileged I/O)
    uint32_t (*pci_cfg_read)(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
    // Read file into buffer. Returns bytes read or -1 on error.
    int32_t (*readfile)(const int8_t* path, uint8_t* outBuf, uint32_t maxLen);
    // Execute ELF image by path with argv and full cmdline. Returns 0 on success, negative on failure.
    int32_t (*exec)(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline);
    // Get process info (for 'top', etc.)
    int32_t (*get_process_info)(char* buffer, int32_t maxlen);
} ApiTableC;

static inline ApiTableC* kos_sys_table(void) {
    return (ApiTableC*)0x0007F000; // fixed address set by kernel
}

static inline void kos_putc(int8_t c) { if (kos_sys_table()->putc) kos_sys_table()->putc(c); }
static inline void kos_puts(const int8_t* s) { if (kos_sys_table()->puts) kos_sys_table()->puts(s); }
static inline void kos_hex(uint8_t v) { if (kos_sys_table()->hex) kos_sys_table()->hex(v); }
static inline void kos_listroot(void) { if (kos_sys_table()->listroot) kos_sys_table()->listroot(); }
static inline void kos_listdir(const int8_t* path) { if (kos_sys_table()->listdir) kos_sys_table()->listdir(path); }
static inline void kos_listdir_ex(const int8_t* path, uint32_t flags) { if (kos_sys_table()->listdir_ex) kos_sys_table()->listdir_ex(path, flags); else if (kos_sys_table()->listdir) kos_sys_table()->listdir(path); }
static inline void kos_clear(void) { if (kos_sys_table()->clear) kos_sys_table()->clear(); }

static inline int32_t kos_mkdir(const int8_t* path, int32_t parents) {
    if (kos_sys_table()->mkdir) return kos_sys_table()->mkdir(path, parents);
    return -1;
}

static inline int32_t kos_chdir(const int8_t* path) {
    if (kos_sys_table()->chdir) return kos_sys_table()->chdir(path);
    return -1;
}

static inline int32_t kos_argc(void) { return kos_sys_table()->get_argc ? kos_sys_table()->get_argc() : 0; }
int snprintf(char *str, size_t size, const char *format, ...);
static inline const int8_t* kos_argv(int32_t index) { return kos_sys_table()->get_arg ? kos_sys_table()->get_arg(index) : (const int8_t*)0; }
static inline const int8_t* kos_cmdline(void) { return kos_sys_table()->cmdline; }
static inline const int8_t* kos_cwd(void) { return kos_sys_table()->cwd; }

// Memory information functions
static inline uint32_t kos_get_total_frames(void) { return kos_sys_table()->get_total_frames ? kos_sys_table()->get_total_frames() : 0; }
static inline uint32_t kos_get_free_frames(void) { return kos_sys_table()->get_free_frames ? kos_sys_table()->get_free_frames() : 0; }
static inline uint32_t kos_get_heap_size(void) { return kos_sys_table()->get_heap_size ? kos_sys_table()->get_heap_size() : 0; }
static inline uint32_t kos_get_heap_used(void) { return kos_sys_table()->get_heap_used ? kos_sys_table()->get_heap_used() : 0; }

// PCI config read wrapper for apps (prevents GP faults by using kernel service)
static inline uint32_t kos_pci_cfg_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    if (kos_sys_table()->pci_cfg_read) return kos_sys_table()->pci_cfg_read(bus, device, function, offset);
    return 0xFFFFFFFFu;
}

// File read wrapper for apps
static inline int32_t kos_readfile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
    if (kos_sys_table()->readfile) return kos_sys_table()->readfile(path, outBuf, maxLen);
    return -1;
}

// Exec wrapper for apps
static inline int32_t kos_exec(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline) {
    if (kos_sys_table()->exec) return kos_sys_table()->exec(path, argc, argv, cmdline);
    return -1;
}

// Flags for kos_listdir_ex
#define KOS_LS_FLAG_LONG  (1u << 0)  // Show long listing: attrs, size, date
#define KOS_LS_FLAG_ALL   (1u << 1)  // Include hidden and dot entries

static inline void kos__print_uint32(size_t v, size_t base, int upper, int width, int padZero) {
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
                if (v < 0) { kos_putc('-'); kos__print_uint32((size_t)(-v), 10, 0, width, padZero); }
                else kos__print_uint32((size_t)v, 10, 0, width, padZero);
                break;
            }
            case 'u': {
                size_t v = va_arg(ap, size_t);
                kos__print_uint32(v, 10, 0, width, padZero);
                break;
            }
            case 'x': {
                size_t v = va_arg(ap, size_t);
                kos__print_uint32(v, 16, 0, width, padZero);
                break;
            }
            case 'X': {
                size_t v = va_arg(ap, size_t);
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