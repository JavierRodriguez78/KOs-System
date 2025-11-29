#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

static void print2(uint8_t v) {
    kos_putc((int8_t)('0' + (v / 10)));
    kos_putc((int8_t)('0' + (v % 10)));
}

static void print4(uint16_t v) {
    kos_putc((int8_t)('0' + ((v / 1000) % 10)));
    kos_putc((int8_t)('0' + ((v / 100) % 10)));
    kos_putc((int8_t)('0' + ((v / 10) % 10)));
    kos_putc((int8_t)('0' + (v % 10)));
}

static void format_and_print_default(uint16_t Y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S) {
    // Default: YYYY-MM-DD HH:MM:SS\n
    print4(Y); kos_putc('-');
    print2(m); kos_putc('-');
    print2(d); kos_putc(' ');
    print2(H); kos_putc(':');
    print2(M); kos_putc(':');
    print2(S); kos_putc('\n');
}

static void print_token(char c, uint16_t Y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S) {
    switch (c) {
        case 'Y': print4(Y); break;
        case 'm': print2(m); break;
        case 'd': print2(d); break;
        case 'H': print2(H); break;
        case 'M': print2(M); break;
        case 'S': print2(S); break;
        case '%': kos_putc('%'); break;
        case 'F': // %Y-%m-%d
            print4(Y); kos_putc('-'); print2(m); kos_putc('-'); print2(d);
            break;
        case 'T': // %H:%M:%S
            print2(H); kos_putc(':'); print2(M); kos_putc(':'); print2(S);
            break;
        default:
            // Unsupported: print literally as in GNU date when unknown token
            kos_putc('%'); kos_putc((int8_t)c);
            break;
    }
}

static void format_custom(const int8_t* fmt, uint16_t Y, uint8_t m, uint8_t d, uint8_t H, uint8_t M, uint8_t S) {
    if (!fmt) return;
    for (int i = 0; fmt[i]; ++i) {
        if (fmt[i] == '%') {
            char n = fmt[i+1];
            if (n) { print_token(n, Y, m, d, H, M, S); ++i; }
            else { kos_putc('%'); }
        } else {
            kos_putc(fmt[i]);
        }
    }
    kos_putc('\n');
}

void app_date(void) {
    // Fetch current RTC time via kernel API
    uint16_t Y; uint8_t m, d, H, M, S;
    kos_get_datetime(&Y, &m, &d, &H, &M, &S);

    int32_t argc = kos_argc();
    // Options: -h/--help, -u (UTC - same as local due to no TZ), +FORMAT
    int32_t i = 1;
    int32_t seen_format = 0;
    for (; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) break;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            kos_puts((const int8_t*)"Usage: date [-u] [+FORMAT]\n");
            kos_puts((const int8_t*)"  -u         Print UTC (same as local in KOS)\n");
            kos_puts((const int8_t*)"  +FORMAT    Output format, supports %Y %m %d %H %M %S, %F, %T\n");
            return;
        }
        if (strcmp(a, (const int8_t*)"-u") == 0) {
            // No timezone handling; RTC assumed local. Ignored for now.
            continue;
        }
        if (a[0] == '+') {
            format_custom(a + 1, Y, m, d, H, M, S);
            seen_format = 1;
            break;
        }
        // Unknown argument; print help
        kos_puts((const int8_t*)"date: unknown option. Try 'date -h'\n");
        return;
    }

    if (!seen_format) {
        format_and_print_default(Y, m, d, H, M, S);
    }
}

#ifndef APP_EMBED
int main(void) { app_date(); return 0; }
#endif
