#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include <arch/x86/hardware/cpu/cpu.hpp>
#include "app.h"

// Simple Linux-like lscpu output for x86
static void print_field(const int8_t* key, const int8_t* val) {
    kos_printf((const int8_t*)"%-18s %s\n", key, val ? val : (const int8_t*)"(null)");
}
static void print_field_u(const int8_t* key, uint32_t v) {
    kos_printf((const int8_t*)"%-18s %u\n", key, v);
}

void app_lscpu(void) {
    // Gather info via CPUID helper
    kos::arch::x86::hardware::cpu::cpu::CpuInfo info;
    kos::arch::x86::hardware::cpu::cpu::GetCpuInfo(info);

    // Architecture and op-modes are fixed for this kernel
    print_field((const int8_t*)"Architecture:", (const int8_t*)"i386");
    print_field((const int8_t*)"CPU op-mode(s):", (const int8_t*)"32-bit");
    print_field((const int8_t*)"Byte Order:", (const int8_t*)"Little Endian");

    // CPU counts (logical vs cores). Our helper sets cores=1.
    print_field_u((const int8_t*)"CPU(s):", info.logical ? info.logical : 1);
    print_field_u((const int8_t*)"On-line CPU(s) list:", info.logical ? info.logical : 1);
    print_field((const int8_t*)"Vendor ID:", info.vendor);
    print_field((const int8_t*)"Model name:", info.brand);
    print_field_u((const int8_t*)"CPU family:", info.family);
    print_field_u((const int8_t*)"Model:", info.model);
    print_field_u((const int8_t*)"Stepping:", info.stepping);

    // Approx MHz (not implemented in cpuid helper; show N/A if zero)
    if (info.mhz) {
        kos_printf((const int8_t*)"%-18s %u.0 MHz\n", (const int8_t*)"CPU MHz:", info.mhz);
    } else {
        print_field((const int8_t*)"CPU MHz:", (const int8_t*)"(unknown)");
    }

    // Flags (simplified): show common 32-bit x86 baseline
    print_field((const int8_t*)"Flags:", (const int8_t*)"fpu de pse tsc msr cx8 sep mtrr pge cmov clflush mmx sse sse2");
}

#ifndef APP_EMBED
int main(void) { app_lscpu(); return 0; }
#endif