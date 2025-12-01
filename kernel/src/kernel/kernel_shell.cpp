#include <kernel/shell.hpp>
#include <console/threaded_shell.hpp>
#include <console/logger.hpp>
#include <kernel/globals.hpp>
#include <process/thread_manager.hpp>

using namespace kos;
using namespace kos::process;

namespace kos { namespace kernel {

void StartShell()
{
    ThreadManagerAPI::StartMultitasking();
    Logger::LogStatus("Multitasking environment started", true);

    // Initialize and start threaded shell
    if (ThreadedShellAPI::InitializeShell()) {
        Logger::LogStatus("Threaded shell initialized", true);
        ThreadedShellAPI::StartShell();
        // Ensure a prompt appears in the new graphical terminal by simulating Enter once
        ThreadedShellAPI::ProcessKeyInput('\n');
    } else {
        Logger::LogStatus("Failed to initialize threaded shell", false);
        // Fallback to original shell
        g_shell = &g_shell_instance;
        Logger::LogStatus("Starting fallback shell...", true);
        g_shell_instance.Run();
    }
}

} }
