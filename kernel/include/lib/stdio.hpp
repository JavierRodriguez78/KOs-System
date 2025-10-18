#ifndef KOS_LIB_STDIO_HPP
#define KOS_LIB_STDIO_HPP

#include <stdarg.h>
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace sys {

        struct ApiTable {
            void (*putc)(int8_t c);
            void (*puts)(const int8_t* s);
            void (*hex)(uint8_t v);
            void (*listroot)();
            void (*listdir)(const int8_t* path);
            void (*listdir_ex)(const int8_t* path, uint32_t flags);
            // Clear text screen
            void (*clear)();
            // Argument passing support
            int32_t (*get_argc)();
            const int8_t* (*get_arg)(int32_t index);
            const int8_t* cmdline; // pointer to full command line (null-terminated)
            // Filesystem (mutating) - currently stubbed in kernel (read-only FS)
            // mkdir(path, parents): create directory; if parents!=0, create intermediate parents like `mkdir -p`.
            // Returns 0 on success, negative on failure.
            int32_t (*mkdir)(const int8_t* path, int32_t parents);
            // Change current working directory. Returns 0 on success.
            int32_t (*chdir)(const int8_t* path);
            // Current working directory path (null-terminated). Set by shell/kernel.
            const int8_t* cwd;
            // Memory information functions
            uint32_t (*get_total_frames)();
            uint32_t (*get_free_frames)();
            uint32_t (*get_heap_size)();
            uint32_t (*get_heap_used)();
            // PCI config space access (read-only): returns a 32-bit value right-shifted by (offset&3)*8
            // Equivalent to reading dword at (offset & ~3) then shifting to align the requested byte/word
            uint32_t (*pci_cfg_read)(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
        };

        // Access to the API table (placed by the kernel at a fixed address)
        ApiTable* table();

        // Non-inline wrappers
        void putc(int8_t c);
        void puts(const int8_t* s);
        void hex(uint8_t v);
        void listroot();
    void listdir(const int8_t* path);
    void listdir_ex(const int8_t* path, uint32_t flags);
    // Clear text screen from apps
    void clear();
    // Accessor used by filesystem printers to customize listing output
    uint32_t CurrentListFlags();

    // Arguments API (for applications)
    int32_t argc();
    const int8_t* argv(int32_t index);
    const int8_t* cmdline();

        // Minimal printf-like output
        void vprintf(const int8_t* fmt, va_list ap);
        void printf(const int8_t* fmt, ...);

        // Optional kernel-side helper wrappers (for kernel code)
        static inline uint32_t pci_cfg_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
            return table()->pci_cfg_read ? table()->pci_cfg_read(bus, device, function, offset) : 0xFFFFFFFFu;
        }

    // Kernel-side utilities exposed here for convenience so users can include a single header
    void SetArgs(int argc, const int8_t** argv, const int8_t* cmdline);
    void SetCwd(const int8_t* path);

    }
}

// Keep C ABI function in global namespace for compatibility
extern "C" void InitSysApi();
#endif