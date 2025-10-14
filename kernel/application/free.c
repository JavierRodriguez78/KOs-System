#include "app.h"
#include "../include/lib/libc/stdio.h"
#include "../include/lib/libc/string.h"

// Print help information
void print_help(void) {
    kos_printf("Usage: free [OPTION]\n");
    kos_printf("Display amount of free and used memory in the system.\n\n");
    kos_printf("Options:\n");
    kos_printf("  -h, --help     display this help and exit\n\n");
    kos_printf("The displayed information includes:\n");
    kos_printf("  Physical:  total and available physical memory frames\n");
    kos_printf("  Heap:      kernel heap size and usage\n\n");
    kos_printf("Memory values are displayed in bytes (B), kilobytes (KiB), or megabytes (MiB).\n");
}

// Print size in human-readable format
void printSize(uint32_t bytes) {
    if (bytes >= (1024 * 1024)) {
        uint32_t mib = bytes / (1024 * 1024);
        kos_printf(" %d MiB", mib);
    } else if (bytes >= 1024) {
        uint32_t kib = bytes / 1024;
        kos_printf(" %d KiB", kib);
    } else {
        kos_printf(" %d B", bytes);
    }
}

void app_free(void) {
    // Check command line arguments
    int32_t argc = kos_argc();
    
    // Parse arguments
    for (int32_t i = 1; i < argc; i++) {
        const int8_t* arg = kos_argv(i);
        if (!arg) continue;
        
        // Check for help flags
        if (strcmp((const char*)arg, "-h") == 0 || strcmp((const char*)arg, "--help") == 0) {
            print_help();
            return;
        } else {
            kos_printf("free: unrecognized option '%s'\n", arg);
            kos_printf("Try 'free --help' for more information.\n");
            return;
        }
    }
    
    kos_puts((const int8_t*)"Memory usage information:\n\n");
    
    // Get physical memory information
    uint32_t totalFrames = kos_get_total_frames();
    uint32_t freeFrames = kos_get_free_frames();
    uint32_t usedFrames = totalFrames - freeFrames;
    
    uint32_t totalPhys = totalFrames * 4096;
    uint32_t freePhys = freeFrames * 4096;
    uint32_t usedPhys = usedFrames * 4096;
    
    // Get heap information
    uint32_t heapTotal = kos_get_heap_size();
    uint32_t heapUsed = kos_get_heap_used();
    uint32_t heapFree = heapTotal - heapUsed;
    
    // Display in Linux free-style format
    kos_puts((const int8_t*)"Physical Memory:\n");
    kos_puts((const int8_t*)"                 total        used        free\n");
    kos_puts((const int8_t*)"Mem:       ");
    printSize(totalPhys);
    printSize(usedPhys);
    printSize(freePhys);
    kos_puts((const int8_t*)"\n\n");
    
    if (heapTotal > 0) {
        kos_puts((const int8_t*)"Kernel Heap:\n");
        kos_puts((const int8_t*)"                 total        used        free\n");
        kos_puts((const int8_t*)"Heap:      ");
        printSize(heapTotal);
        printSize(heapUsed);
        printSize(heapFree);
        kos_puts((const int8_t*)"\n\n");
    }
    
    // Show detailed frame information
    kos_printf((const int8_t*)"Physical frames: %u total, %u used, %u free (4 KiB each)\n", 
               totalFrames, usedFrames, freeFrames);
    
    // Calculate percentages
    uint32_t physUsedPercent = totalFrames > 0 ? (usedFrames * 100) / totalFrames : 0;
    uint32_t heapUsedPercent = heapTotal > 0 ? (heapUsed * 100) / heapTotal : 0;
    
    kos_printf((const int8_t*)"Physical memory usage: %u%%\n", physUsedPercent);
    if (heapTotal > 0) {
        kos_printf((const int8_t*)"Kernel heap usage: %u%%\n", heapUsedPercent);
    }
}

int main(void) {
    app_free();
    return 0;
}