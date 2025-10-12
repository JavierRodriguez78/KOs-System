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
                // List contents of a directory by absolute path ("/" to list root). Default falls back to ListRoot.
                virtual void ListDir(const int8_t* path) { (void)path; ListRoot(); }
                // Minimal read API: read a file by absolute path into provided buffer.
                // Returns number of bytes read, or -1 on error.
                virtual int32_t ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) { 
                    (void)path; 
                    (void)outBuf; 
                    (void)maxLen; 
                    return -1; 
                }
                // Create directory. If parents!=0, create intermediate directories.
                // Returns 0 on success, negative on failure.
                virtual int32_t Mkdir(const int8_t* path, int32_t parents) { (void)path; (void)parents; return -1; }
        };      
    }
}

#endif
