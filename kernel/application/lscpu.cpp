#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.hpp"

static void print_field(const int8_t* key, const int8_t* val) {
    // kos_printf does not support '-' flag or width for strings; pad manually
    const int padTo = 18;
    // print key
    kos_puts(key);
    // compute length of key
    int len = 0; while (key && key[len]) ++len;
    // add spaces
    for (int i = len; i < padTo; ++i) kos_putc(' ');
    kos_putc(' ');
    kos_puts(val ? val : (const int8_t*)"(null)");
    kos_putc('\n');
}
static void print_field_u(const int8_t* key, uint32_t v) {
    const int padTo = 18;
    kos_puts(key);
    int len = 0; while (key && key[len]) ++len;
    for (int i = len; i < padTo; ++i) kos_putc(' ');
    kos_putc(' ');
    kos_printf((const int8_t*)"%u\n", v);
}

// Minimal CPUID helpers (inline) to avoid dependency mismatches
static inline int cpuid_available() {
#if defined(__GNUC__) && defined(__i386__)
    int supported = 0;
    __asm__ __volatile__ (
        "pushf\n\t"
        "pop %%eax\n\t"
        "mov %%eax, %%ecx\n\t"
        "xor $0x200000, %%eax\n\t"
        "push %%eax\n\t"
        "popf\n\t"
        "pushf\n\t"
        "pop %%eax\n\t"
        "xor %%ecx, %%eax\n\t"
        "setz %%al\n\t"
        "movzx %%al, %0\n\t"
        : "=r"(supported)
        :
        : "eax", "ecx"
    );
    return supported;
#else
    return 0;
#endif
}

static inline void cpuid_regs(uint32_t eax, uint32_t ecx, uint32_t* regs) {
#if defined(__GNUC__) && defined(__i386__)
    __asm__ __volatile__(
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(eax), "c"(ecx)
    );
#else
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

extern "C" void app_lscpu(void) {
    // Gather info directly via CPUID (fallbacks if unavailable)
    int have_cpuid = cpuid_available();
    int8_t vendor[13]; for (int i=0;i<13;++i) vendor[i]=0;
    int8_t brand[49]; for (int i=0;i<49;++i) brand[i]=0;
    uint32_t family=0, model=0, stepping=0, logical=1, cores=1;
    if (have_cpuid) {
        uint32_t r[4];
        cpuid_regs(0, 0, r);
        ((uint32_t*)vendor)[0] = r[1];
        ((uint32_t*)vendor)[1] = r[3];
        ((uint32_t*)vendor)[2] = r[2];
        vendor[12] = 0;
        cpuid_regs(1, 0, r);
        family = ((r[0] >> 8) & 0xF) + ((r[0] >> 20) & 0xFF);
        model = ((r[0] >> 4) & 0xF) + ((r[0] >> 12) & 0xF0);
        stepping = (r[0] & 0xF);
        // Logical processors per package reported in EBX[23:16]
        // Some VMs may not set HTT bit; still trust EBX count if nonzero.
        logical = ((r[1] >> 16) & 0xFF);
        if (logical == 0) logical = 1;
        // Try to get core count via CPUID leaf 4 (Intel): EAX[31:26]+1 for each subleaf 0
        uint32_t r4[4];
        cpuid_regs(4, 0, r4);
        uint32_t cores_plus_minus1 = (r4[0] >> 26) & 0x3F; // Core count minus 1
        cores = (cores_plus_minus1 + 1) ? (cores_plus_minus1 + 1) : 1;
        // If leaf 4 isn't supported (all zeros), fallback to logical
        if (r4[0] == 0 && r4[1] == 0 && r4[2] == 0 && r4[3] == 0) cores = logical;
        cpuid_regs(0x80000002, 0, (uint32_t*)&brand[0]);
        cpuid_regs(0x80000003, 0, (uint32_t*)&brand[16]);
        cpuid_regs(0x80000004, 0, (uint32_t*)&brand[32]);
        brand[48] = 0;
    }

    print_field((const int8_t*)"Architecture:", (const int8_t*)"i386");
    print_field((const int8_t*)"CPU op-mode(s):", (const int8_t*)"32-bit");
    print_field((const int8_t*)"Byte Order:", (const int8_t*)"Little Endian");

    // Report total logical CPUs from CPUID
    print_field_u((const int8_t*)"CPU(s):", logical);
    // Show cores detail
    print_field_u((const int8_t*)"Core(s) per socket:", cores);
    // Online CPUs: assume all logical CPUs are online for display
    // Format as 0-(logical-1)
    char online_buf[16];
    if (logical > 1) {
        int n = snprintf((char*)online_buf, sizeof(online_buf), "0-%u", (unsigned)(logical-1));
        (void)n;
        print_field((const int8_t*)"On-line CPU(s) list:", (const int8_t*)online_buf);
    } else {
        print_field((const int8_t*)"On-line CPU(s) list:", (const int8_t*)"0");
    }
    print_field((const int8_t*)"Vendor ID:", have_cpuid ? vendor : (const int8_t*)"(unknown)");
    print_field((const int8_t*)"Model name:", have_cpuid ? brand : (const int8_t*)"(unknown)");
    print_field_u((const int8_t*)"CPU family:", family);
    print_field_u((const int8_t*)"Model:", model);
    print_field_u((const int8_t*)"Stepping:", stepping);

        // CPU MHz via TSC measured against RTC second tick
        {
            auto rdtsc = []() -> uint64_t {
                uint32_t lo, hi;
                __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
                return ((uint64_t)hi << 32) | lo;
            };
            auto udiv64by32 = [](uint64_t n, uint32_t d) -> uint32_t {
                // Binary long division: compute 64/32 -> 32 quotient
                if (d == 0) return 0;
                uint64_t rem = 0;
                uint32_t q = 0;
                for (int i = 63; i >= 0; --i) {
                    rem = (rem << 1) | ((n >> i) & 1ULL);
                    if (rem >= d) { rem -= d; q |= (1u << i); }
                }
                return q;
            };
            uint16_t y; uint8_t mo, d, h, mi, s;
            kos_get_datetime(&y, &mo, &d, &h, &mi, &s);
            uint8_t start_sec = s;
            int guard = 0;
            while (1) {
                kos_get_datetime(&y, &mo, &d, &h, &mi, &s);
                if (s != start_sec) break;
                if (++guard > 5000000) break;
            }
            uint64_t t0 = rdtsc();
            uint8_t sec0 = s;
            guard = 0;
            while (1) {
                kos_get_datetime(&y, &mo, &d, &h, &mi, &s);
                if (s != sec0) break;
                if (++guard > 10000000) break;
            }
            uint64_t t1 = rdtsc();
            uint64_t cycles = (t1 >= t0) ? (t1 - t0) : 0ULL;
            if (cycles < 100000000ULL) {
                uint64_t ta = rdtsc();
                for (volatile int i = 0; i < 5000000; ++i) { }
                uint64_t tb = rdtsc();
                uint64_t dc = (tb >= ta) ? (tb - ta) : 0ULL;
                if (dc > 0ULL) cycles = dc * 5ULL; // ~200ms scaled to 1s
            }
            uint32_t mhz = udiv64by32(cycles, 1000000u);
            char buf[32];
            int n = snprintf((char*)buf, sizeof(buf), "%u.0", mhz);
            (void)n;
            print_field((const int8_t*)"CPU MHz:", buf);
        }

    // Minimal flags decoding from CPUID(1): EDX bits and ECX bits (subset)
    if (have_cpuid) {
        uint32_t r[4]; cpuid_regs(1, 0, r);
        uint32_t edx = r[3]; uint32_t ecx = r[2];
        // Build a flags string into a small buffer
        const int MAX = 256; int8_t buf[MAX]; int bi=0;
        auto add = [&](const char* s){ for (int i=0; s[i] && bi < MAX-2; ++i) buf[bi++] = s[i]; buf[bi++]=' '; };
        if (edx & (1u<<0)) add("fpu");
        if (edx & (1u<<3)) add("pse");
        if (edx & (1u<<4)) add("tsc");
        if (edx & (1u<<5)) add("msr");
        if (edx & (1u<<8)) add("cx8");
        if (edx & (1u<<9)) add("apic");
        if (edx & (1u<<12)) add("mtrr");
        if (edx & (1u<<13)) add("pge");
        if (edx & (1u<<15)) add("cmov");
        if (edx & (1u<<19)) add("clflush");
        if (edx & (1u<<23)) add("mmx");
        if (edx & (1u<<25)) add("sse");
        if (edx & (1u<<26)) add("sse2");
        if (ecx & (1u<<0)) add("sse3");
        if (ecx & (1u<<9)) add("ssse3");
        if (ecx & (1u<<19)) add("sse4.1");
        if (ecx & (1u<<20)) add("sse4.2");
        if (ecx & (1u<<28)) add("avx");
        buf[bi] = 0;
        print_field((const int8_t*)"Flags:", bi ? buf : (const int8_t*)"(none)");
    } else {
        print_field((const int8_t*)"Flags:", (const int8_t*)"(unknown)");
    }
}

#ifndef APP_EMBED
extern "C" int main(void) { app_lscpu(); return 0; }
#endif