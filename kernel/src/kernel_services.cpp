#include <kernel/services.hpp>
#include <services/service_manager.hpp>
#include <services/banner_service.hpp>
#include <services/time_service.hpp>
#include <services/filesystem_service.hpp>
#include <services/window_manager.hpp>
#include <services/service.hpp>
#include <console/logger.hpp>
#include <kernel/globals.hpp>
#include <lib/elfloader.hpp>
#include <process/thread_manager.hpp>
#include <application/init/service.hpp>

using namespace kos;
using namespace kos::services;
using namespace kos::fs;

namespace kos { namespace kernel {

static BannerService g_banner_service;
static TimeService g_time_service;
static kos::services::WindowManager g_window_manager;
static FilesystemService g_fs_service;
static kos::services::InitDService g_initd_service;

void RegisterAndStartServices()
{
    // Register built-in services and start them based on configuration
    ServiceManager::Register(&g_fs_service);
    ServiceManager::Register(&g_banner_service);
    ServiceManager::Register(&g_time_service);
    ServiceManager::Register(&g_window_manager);
    ServiceManager::Register(&g_initd_service);
    Logger::Log("Kernel: services registered (FS, BANNER, TIME, WINMAN, INITD)");
    // Apply mouse poll mode before WindowManager starts
    WindowManager::SetMousePollMode(kos::g_mouse_poll_mode);
    ServiceManager::InitAndStart();
    Logger::Log("Kernel: services started");
    ServiceAPI::StartManagerThread();

    // If present, spawn /bin/init as the first userspace process (assign PID 1)
    if (g_fs_ptr) {
        uint32_t pid1 = 0;
        uint32_t tid = ThreadManagerAPI::SpawnProcess("/bin/init.elf", "init", &pid1, 8192, PRIORITY_NORMAL, 0);
        if (tid) {
            Logger::LogKV("Kernel: spawned init (thread)", "ok");
            Logger::LogKV("init PID", (pid1 ? "1" : "?"));
        }
    }
}

} }
