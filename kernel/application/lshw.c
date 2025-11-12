// lshw-like hardware info printer for KOS
// Prints CPU brand, vendor, features; memory stats; and basic PCI device list
#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

// Inline cpuid helper (i386)
static void do_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t regs[4]) {
    // regs: EAX, EBX, ECX, EDX
    uint32_t a, b, c, d;
    __asm__ __volatile__ ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
    regs[0] = a; regs[1] = b; regs[2] = c; regs[3] = d;
}

static void print_cpuid_info(void) {
    uint32_t r[4];
    // Vendor string from leaf 0
    do_cpuid(0, 0, r);
    uint32_t max_basic = r[0];
    char vendor[13];
    ((uint32_t*)vendor)[0] = r[1]; // EBX
    ((uint32_t*)vendor)[1] = r[3]; // EDX
    ((uint32_t*)vendor)[2] = r[2]; // ECX
    vendor[12] = 0;

    kos_printf((const int8_t*)"CPU Vendor: %s\n", (const int8_t*)vendor);

    // Brand string from extended leaves 0x80000002..0x80000004
    do_cpuid(0x80000000u, 0, r);
    uint32_t max_ext = r[0];
    char brand[49];
    for (int i = 0; i < 48; ++i) brand[i] = '\0'; brand[48] = 0;
    if (max_ext >= 0x80000004u) {
        uint32_t* p = (uint32_t*)brand;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
            do_cpuid(leaf, 0, r);
            *p++ = r[0]; *p++ = r[1]; *p++ = r[2]; *p++ = r[3];
        }
    }
    // Trim leading spaces in brand
    int idx = 0; while (brand[idx] == ' ') idx++;
    kos_printf((const int8_t*)"CPU Brand:  %s\n", (const int8_t*)(brand + idx));

    // Family/Model/Stepping from leaf 1
    if (max_basic >= 1) {
        do_cpuid(1, 0, r);
        uint32_t eax = r[0];
        uint32_t ebx = r[1];
        uint32_t ecx = r[2];
        uint32_t edx = r[3];
        uint32_t stepping = eax & 0xF;
        uint32_t model = (eax >> 4) & 0xF;
        uint32_t family = (eax >> 8) & 0xF;
        uint32_t ext_model = (eax >> 16) & 0xF;
        uint32_t ext_family = (eax >> 20) & 0xFF;
        if (family == 0xF) family += ext_family;
        if (family == 0x6 || family == 0xF) model += (ext_model << 4);
        uint32_t cpu_count = (ebx >> 16) & 0xFF;
        kos_printf((const int8_t*)"CPU Family: %u  Model: %u  Stepping: %u  Logical CPUs: %u\n",
                   family, model, stepping, cpu_count);
        // Some notable features
        kos_puts((const int8_t*)"CPU Features:");
        if (edx & (1u<<23)) kos_puts((const int8_t*)" MMX");
        if (edx & (1u<<25)) kos_puts((const int8_t*)" SSE");
        if (edx & (1u<<26)) kos_puts((const int8_t*)" SSE2");
        if (ecx & (1u<<0))  kos_puts((const int8_t*)" SSE3");
        if (ecx & (1u<<9))  kos_puts((const int8_t*)" SSSE3");
        if (ecx & (1u<<19)) kos_puts((const int8_t*)" SSE4.1");
        if (ecx & (1u<<20)) kos_puts((const int8_t*)" SSE4.2");
        if (ecx & (1u<<28)) kos_puts((const int8_t*)" AVX");
        kos_puts((const int8_t*)"\n");
    }
}

static inline uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return kos_pci_cfg_read(bus, device, function, offset);
}

static void print_pci_devices(void) {
    kos_puts((const int8_t*)"PCI Devices:\n");
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            uint8_t functions = 1;
            uint8_t header_type = (uint8_t)pci_config_read(bus, dev, 0, 0x0E);
            if (header_type & 0x80) functions = 8;
            for (uint8_t fn = 0; fn < functions; ++fn) {
                uint16_t vendor = (uint16_t)pci_config_read(bus, dev, fn, 0x00);
                if (vendor == 0xFFFF) { if (fn == 0) break; else continue; }
                uint16_t device = (uint16_t)pci_config_read(bus, dev, fn, 0x02);
                uint8_t class_id = (uint8_t)pci_config_read(bus, dev, fn, 0x0B);
                uint8_t subclass = (uint8_t)pci_config_read(bus, dev, fn, 0x0A);
                uint8_t prog_if = (uint8_t)pci_config_read(bus, dev, fn, 0x09);
                uint8_t int_line = (uint8_t)pci_config_read(bus, dev, fn, 0x3C);
                kos_printf((const int8_t*)"  %02X:%02X.%u  vendor=%04X device=%04X class=%02X subclass=%02X progIF=%02X irq=%02X\n",
                           bus, dev, fn, vendor, device, class_id, subclass, prog_if, int_line);
            }
        }
    }
}

// Decode a few known network devices for nicer lshw output
static const char* decode_net_name(uint16_t vendor, uint16_t device) {
    if (vendor == 0x8086 && device == 0x100E) return "Intel 82540EM (e1000)"; // QEMU/VirtualBox
    if (vendor == 0x10EC) {
        if (device == 0x8139) return "Realtek RTL8139";
        if (device == 0x8169) return "Realtek RTL8169";
        if (device == 0x8168) return "Realtek RTL8168/8111";
        if (device == 0x8136) return "Realtek RTL8101/8102 FE";
    }
    // Add more pairs here as you add drivers
    return "Unknown NIC";
}

static void print_net_devices(void) {
    kos_puts((const int8_t*)"Network adapters:\n");
    int found = 0;
    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            uint8_t functions = 1;
            uint8_t header_type = (uint8_t)pci_config_read(bus, dev, 0, 0x0E);
            if (header_type & 0x80) functions = 8;
            for (uint8_t fn = 0; fn < functions; ++fn) {
                uint16_t vendor = (uint16_t)pci_config_read(bus, dev, fn, 0x00);
                if (vendor == 0xFFFF) { if (fn == 0) break; else continue; }
                uint16_t device = (uint16_t)pci_config_read(bus, dev, fn, 0x02);
                uint8_t class_id = (uint8_t)pci_config_read(bus, dev, fn, 0x0B);
                if (class_id != 0x02) continue; // 0x02 = Network Controller
                uint8_t subclass = (uint8_t)pci_config_read(bus, dev, fn, 0x0A);
                uint8_t int_line = (uint8_t)pci_config_read(bus, dev, fn, 0x3C);
                uint32_t bar0 = (uint32_t)pci_config_read(bus, dev, fn, 0x10);
                const char* name = decode_net_name(vendor, device);
                // Determine BAR type and base
                int is_io = (bar0 & 0x1) ? 1 : 0;
                uint32_t base = is_io ? (bar0 & 0xFFFFFFFCu) : (bar0 & 0xFFFFFFF0u);
                kos_printf((const int8_t*)"  %02X:%02X.%u  %s  vendor=%04X device=%04X subclass=%02X  IRQ=%u\n",
                           bus, dev, fn, (const int8_t*)name, vendor, device, subclass, int_line);
                kos_printf((const int8_t*)"    BAR0: %s @ 0x%X\n", is_io ? (const int8_t*)"I/O" : (const int8_t*)"MMIO", base);
                found = 1;
            }
        }
    }
    if (!found) kos_puts((const int8_t*)"  (none)\n");
}

static void print_memory_info(void) {
    uint32_t total_frames = kos_get_total_frames();
    uint32_t free_frames = kos_get_free_frames();
    uint32_t heap_size = kos_get_heap_size();
    uint32_t heap_used = kos_get_heap_used();
    // Convert frames (assumed 4KiB) to KiB/ MiB
    uint32_t frame_kib = total_frames * 4;
    uint32_t free_kib = free_frames * 4;
    kos_puts((const int8_t*)"Memory:\n");
    kos_printf((const int8_t*)"  Physical: %u KiB total, %u KiB free (%u used)\n",
               frame_kib, free_kib, (frame_kib - free_kib));
    kos_printf((const int8_t*)"  Kernel heap: %u bytes total, %u bytes used\n",
               heap_size, heap_used);
}

void app_lshw(void) {
    kos_puts((const int8_t*)"KOS Hardware Information\n");
    kos_puts((const int8_t*)"------------------------\n");
    print_cpuid_info();
    kos_puts((const int8_t*)"\n");
    print_memory_info();
    kos_puts((const int8_t*)"\n");
    print_net_devices();
    kos_puts((const int8_t*)"\n");
    print_pci_devices();
}

#ifndef APP_EMBED
int main(void) {
    app_lshw();
    return 0;
}
#endif
