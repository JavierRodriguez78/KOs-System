#include <fs/filesystem.hpp>
#include <console/shell.hpp>
#include <lib/string.hpp>


using namespace kos::fs;
using namespace kos::console;
using namespace kos::lib;

namespace kos{
    namespace fs{
        bool Filesystem::Exists(const int8_t* path) {
            // Simple implementation: only check for root directory
            if (String::strcmp(path, "/bin/", 5) == 0) {
                const int8_t* cmd = path + 5;
                return CommandRegistry::Find(cmd) != nullptr;
            }
            return false;
        };

        void* Filesystem::GetCommandEntry(const int8_t* path) {
            if (String::strcmp(path, "/bin/", 5) == 0) {
                const int8_t* cmd = path + 5;
                return (void*)CommandRegistry::Find(cmd);
            }
            return nullptr;
        };
    }

    // Stub for WriteFile in base Filesystem (does nothing)
    int32_t Filesystem::WriteFile(const int8_t* path, const uint8_t* data, uint32_t len) {
        (void)path;
        (void)data;
        (void)len;
        return -1;
    }
}

namespace kos { namespace fs { Filesystem* g_fs_ptr = nullptr; } }