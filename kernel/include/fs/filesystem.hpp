#ifndef __KOS__FS__FILESYSTEM_H
#define __KOS__FS__FILESYSTEM_H

#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace fs {
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
        };
        extern Filesystem* g_fs_ptr;
        // ...existing code...
    }
}

#endif
