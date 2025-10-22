#include <services/filesystem_service.hpp>
#include <console/logger.hpp>
#include <fs/filesystem.hpp>

using namespace kos::console;

// Access global filesystem selected during hardware init (defined in kernel.cpp)
extern kos::fs::Filesystem* g_fs_ptr;

namespace kos {
    namespace services {

bool FilesystemService::Start() {
    Logger::Log("FilesystemService: starting");
    if (!g_fs_ptr) {
        Logger::Log("FilesystemService: no filesystem present (skipping)");
        return false; // keep as failure so other services can react accordingly
    }

    // Ensure standard directories exist; best-effort
    const int8_t* dirs[] = {
        (const int8_t*)"/BIN",
        (const int8_t*)"/HOME",
        (const int8_t*)"/ETC",
        (const int8_t*)"/ETC/INIT.D",
    };
    for (unsigned i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
        if (!g_fs_ptr->DirExists(dirs[i])) {
            int32_t rc = g_fs_ptr->Mkdir(dirs[i], /*parents*/1);
            Logger::LogKV("FilesystemService: mkdir", (rc == 0) ? "ok" : "fail");
        }
    }

    Logger::Log("FilesystemService: ready");
    return true;
}

}} // namespace
