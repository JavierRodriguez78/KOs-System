#pragma once
#ifndef KOS_LIB_STDIO_HPP
#define KOS_LIB_STDIO_HPP

#include <stdarg.h>
#include <common/types.hpp>
#include <lib/stddef.hpp>

using namespace kos::common;

namespace kos { 
    namespace sys {
        /*
        *@brief System API function table
        */
        struct ApiTable {
            // Basic I/O functions
            void (*putc)(int8_t c);
            /*
            * @brief Non-inline wrapper for putc.
            * @param c Character to output.
            */
            void (*puts)(const int8_t* s);
            /*
            * @brief Non-inline wrapper for puts.
            * @param s String to output.
            */
            void (*hex)(uint8_t v);
            /*
            * @brief Non-inline wrapper for hex.
            * @param v Value to output in hex.
            */  
            void (*listroot)();
            void (*listdir)(const int8_t* path);
            void (*listdir_ex)(const int8_t* path, uint32_t flags);
            // Clear text screen
            void (*clear)();
                void (*set_attr)(uint8_t attr);
                void (*set_color)(uint8_t fg, uint8_t bg);
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

            // New: read a file into buffer. Returns bytes read or -1 on error.
            int32_t (*readfile)(const int8_t* path, uint8_t* outBuf, uint32_t maxLen);
            // New: execute an ELF at path with argv and cmdline. Returns 0 on success, negative on failure.
            int32_t (*exec)(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline);
            // Get process info (for 'top', etc.)
            int32_t (*get_process_info)(char* buffer, int32_t maxlen);
            // Non-blocking key poll for apps: returns 1 if a key was read into *out, 0 otherwise
            int32_t (*key_poll)(int8_t* out);
            // Get current date/time from RTC (local time). All pointers optional.
            void (*get_datetime)(uint16_t* year, uint8_t* month, uint8_t* day,
                                 uint8_t* hour, uint8_t* minute, uint8_t* second);
            // Rename/move a file or directory: src -> dst. Returns 0 on success, negative on failure.
            int32_t (*rename)(const int8_t* src, const int8_t* dst);
            // Enumerate sockets (future TCP/UDP stack). Returns count or <0 on error.
            // Kept as opaque pointer signature for C/C++ compatibility with ApiTableC.
            int32_t (*net_list_sockets)(void* out, int32_t max, int32_t want_tcp, int32_t want_udp, int32_t listening_only);
        };

        /*
        * @brief Get a pointer to the API table.
        * @return Pointer to the API table.
        * Access to the API table (placed by the kernel at a fixed address)
        */
        ApiTable* table();

        // Non-inline wrappers
       /*
       * @brief Non-inline wrapper for putc.
       * @param c Character to output.
       */
        void putc(int8_t c);
        /*
        * @brief Non-inline wrapper for puts.
        * @param s String to output.
        */
        void puts(const int8_t* s);
        /*
        * @brief Non-inline wrapper for hex.
        * @param v Value to output in hex.
        */
        void hex(uint8_t v);
        /*
        * @brief Non-inline wrapper for listroot.
        */
        void listroot();

        /*
        * @brief Non-inline wrapper for listdir.
        * @param path Path to directory.
        */  
        void listdir(const int8_t* path);

        /*
        * @brief Non-inline wrapper for listdir_ex.
        * @param path Path to directory.
        * @param flags Flags for listing options.
        */
        void listdir_ex(const int8_t* path, uint32_t flags);

        /*
        * @brief Non-inline wrapper for clear.
        */
        void clear();

        /*
        * @brief Non-inline wrapper for CurrentListFlags.
        * @return Current listing flags.
        */
        uint32_t CurrentListFlags();

        // Arguments API (for applications)
    
        /*
        * @brief Get the argument count.
        * @return Number of arguments.
        */
        int32_t argc();
        
        /*
        * @brief Get the argument at the specified index.
        * @param index Argument index (0-based).
        * @return Pointer to the argument string, or nullptr if out of bounds.
        */
        const int8_t* argv(int32_t index);
    
        /*
        * @brief Get the command line string.
        * @return Pointer to the command line string.
        */
        const int8_t* cmdline();

        // Minimal printf-like output
        /*
        * @brief Print formatted output to the console.
        * @param fmt Format string.
        * @param ap Argument list.
        */
        void vprintf(const int8_t* fmt, va_list ap);
        void printf(const int8_t* fmt, ...);

        
        /*
        * @brief Minimal keyboard-backed scanf.
        * Supports %d, %u, %x/%X, %s, %c with basic whitespace handling.
        * @return Number of successfully assigned items.
        * @params... Variable argument list to store the input values.  
        */
        int scanf(const int8_t* fmt, ...);

        /*
        * @brief Try to deliver a key to the input consumer.
        * @param c Key to deliver.
        * @return True if the key was consumed, false otherwise.
        * Internal: keyboard handler can offer a key to stdio input consumer.
        */
        bool TryDeliverKey(int8_t c);

        
        // Optional kernel-side helper wrappers (for kernel code)
        static inline uint32_t pci_cfg_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
            return table()->pci_cfg_read ? table()->pci_cfg_read(bus, device, function, offset) : 0xFFFFFFFFu;
        }

    // Kernel-side utilities exposed here for convenience so users can include a single header
    void SetArgs(int argc, const int8_t** argv, const int8_t* cmdline);
    void SetCwd(const int8_t* path);

    // Kernel-side helpers (if needed) to call into API
    static inline int32_t readfile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
        return table()->readfile ? table()->readfile(path, outBuf, maxLen) : -1;
    }
    static inline int32_t exec(const int8_t* path, int32_t argc, const int8_t** argv, const int8_t* cmdline) {
        return table()->exec ? table()->exec(path, argc, argv, cmdline) : -1;
    }
    // No inline wrapper for key_poll on kernel side (apps use libc header)
        static inline void get_datetime(uint16_t* year, uint8_t* month, uint8_t* day,
                                        uint8_t* hour, uint8_t* minute, uint8_t* second) {
            if (table()->get_datetime) table()->get_datetime(year, month, day, hour, minute, second);
        }
    
    /*
    * @brief Format a string and store it in a buffer.
    * @param str Destination buffer.
    * @param size Size of the buffer.
    * @param format Format string.
    * @return Number of characters written (excluding null terminator), or -1 on error.
    */
    int snprintf(char *str, size_t size, const char *format, ...);

    }
}

// Keep C ABI function in global namespace for compatibility
extern "C" void InitSysApi();





#endif