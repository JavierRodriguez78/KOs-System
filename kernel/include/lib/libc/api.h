#ifndef KOS_LIBC_API_H
#define KOS_LIBC_API_H

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
} ApiTableC;

ApiTableC* kos_sys_table(void);
void kos_putc(int8_t c);
void kos_puts(const int8_t* s);
void kos_hex(uint8_t v);
void kos_listroot(void);

void kos_vprintf(const int8_t* fmt, va_list ap);
void kos_printf(const int8_t* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
