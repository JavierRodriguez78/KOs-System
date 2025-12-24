#ifndef __KOS__FS__FILESYSTEM_H
#define __KOS__FS__FILESYSTEM_H

#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace fs {
        // Directory entry structure for enumeration
        struct DirEntry {
            int8_t name[13];    // 8.3 name + null terminator
            uint32_t size;      // File size in bytes (0 for directories)
            uint8_t attr;       // FAT attributes (0x10 = directory)
            bool isDir;         // Convenience flag
        };
        
        // Callback type for directory enumeration
        // Returns true to continue enumeration, false to stop
        typedef bool (*DirEnumCallback)(const DirEntry* entry, void* userdata);
        
        class Filesystem {
        public:
            virtual ~Filesystem() {}
            virtual bool Mount() = 0;
            virtual void ListRoot() = 0;
            virtual void DebugInfo() = 0;
            static bool Exists(const int8_t* path);
            static void* GetCommandEntry(const int8_t* path);
            virtual void ListDir(const int8_t* path) { (void)path; ListRoot(); }
            virtual bool DirExists(const int8_t* path) { return path && path[0] == '/' && path[1] == 0; }
            virtual int32_t ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
                (void)path;
                (void)outBuf;
                (void)maxLen;
                return -1;
            }
            virtual int32_t WriteFile(const int8_t* path, const uint8_t* data, uint32_t len);
            virtual int32_t Mkdir(const int8_t* path, int32_t parents) { (void)path; (void)parents; return -1; }
            // Rename or move a file or directory. Default: unsupported (-1).
            // Paths are absolute and normalized by caller.
            virtual int32_t Rename(const int8_t* src, const int8_t* dst) { (void)src; (void)dst; return -1; }
            // Enumerate directory entries. Calls callback for each entry.
            // Returns number of entries enumerated, or -1 on error.
            virtual int32_t EnumDir(const int8_t* path, DirEnumCallback callback, void* userdata) {
                (void)path; (void)callback; (void)userdata; return -1;
            }
        };
        extern Filesystem* g_fs_ptr;
        // ...existing code...
    }
}

#endif
