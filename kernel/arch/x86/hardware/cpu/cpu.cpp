
// cpu.cpp - x86 CPU info and stats for KOS
#include <arch/x86/hardware/cpu/cpu.hpp>
#include <lib/libc/stdio.h>
#include <lib/string.hpp>


using namespace kos::lib;
using namespace kos::arch::x86::hardware::cpu;

static void cpuid(uint32_t eax, uint32_t ecx, uint32_t* regs) {
                bool cpuid_ok = true;
                #if defined(__GNUC__) && defined(__i386__)
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
                "jz 1f\n\t"
                "mov $1, %[ok]\n\t"
                "1:"
                : [ok] "=r"(cpuid_ok)
                :
                : "eax", "ecx"
            );
            if (cpuid_ok) {
                __asm__ __volatile__ (
                    "cpuid"
                    : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                    : "a"(eax), "c"(ecx)
                );
            } else {
                regs[0] = regs[1] = regs[2] = regs[3] = 0;
            }
        #else
        regs[0] = regs[1] = regs[2] = regs[3] = 0;
    #endif
}

void GetCpuInfo(CpuInfo& info) {
    uint32_t regs[4];
    cpuid(0, 0, regs);
    ((uint32_t*)info.vendor)[0] = regs[1];
    ((uint32_t*)info.vendor)[1] = regs[3];
    ((uint32_t*)info.vendor)[2] = regs[2];
    info.vendor[12] = 0;

    cpuid(1, 0, regs);
    info.family = ((regs[0] >> 8) & 0xf) + ((regs[0] >> 20) & 0xff);
    info.model = ((regs[0] >> 4) & 0xf) + ((regs[0] >> 12) & 0xf0);
    info.stepping = regs[0] & 0xf;
    info.logical = (regs[1] >> 16) & 0xff;
    info.cores = 1; // For simplicity, single core
    info.mhz = 0; // Not available via CPUID

    cpuid(0x80000002, 0, (uint32_t*)&info.brand[0]);
    cpuid(0x80000003, 0, (uint32_t*)&info.brand[16]);
    cpuid(0x80000004, 0, (uint32_t*)&info.brand[32]);
    info.brand[48] = 0;
}

void PrintCpuInfo() {
    CpuInfo info;
    GetCpuInfo(info);
    if (info.vendor[0] == 0 && info.brand[0] == 0) {
        kos_puts("[WARN] CPUID not supported or invalid opcode.\n");
        kos_puts("CPU Info: (unavailable)\n");
    } else {
        kos_puts("CPU Info:\n");
        kos_printf("  Vendor: %s\n", info.vendor);
        kos_printf("  Brand: %s\n", info.brand);
        kos_printf("  Family: %u\n", info.family);
        kos_printf("  Model: %u\n", info.model);
        kos_printf("  Stepping: %u\n", info.stepping);
        kos_printf("  Logical CPUs: %u\n", info.logical);
        kos_printf("  Cores: %u\n", info.cores);
    }
}

extern "C" void PrintCpuInfo_C() {
    kos::arch::x86::PrintCpuInfo();
}