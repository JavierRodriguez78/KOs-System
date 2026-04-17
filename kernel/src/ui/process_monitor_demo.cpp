#include <ui/framework.hpp>
#include <ui/process_viewer.hpp>
#include <graphics/compositor.hpp>
#include <console/logger.hpp>
#include <process/scheduler.hpp>

using namespace kos::ui;
using namespace kos::gfx;
using namespace kos::process;

namespace kos { namespace ui {

/**
 * ProcessMonitorDemo - Demonstration of the Process Monitor GUI
 * 
 * This creates a graphical window showing system processes running
 * and can be invoked from the kernel's graphical environment.
 */
class ProcessMonitorDemo {
public:
    // Initialize and run the process monitor demo
    static void Run() {
        if (!IsAvailable()) {
            Logger::Log("[ProcessMonitor] Skipping - no graphical environment");
            return;
        }
        
        Logger::Log("[ProcessMonitor] Initializing Process Monitor");
        
        // Create window for process list
        uint32_t windowId = CreateWindow(
            60,          // x
            60,          // y
            700,         // width
            380,         // height
            0xFF1F1F2E,  // bg color
            "System Process Monitor",
            WF_Resizable | WF_Minimizable | WF_Maximizable | WF_Closable
        );
        
        if (windowId == 0) {
            Logger::Log("[ProcessMonitor] Failed to create window");
            return;
        }
        
        // Initialize process visualization
        ProcessViewer::Initialize(windowId);
        ProcessViewer::RefreshProcessList();
        
        Logger::Log("[ProcessMonitor] Process Monitor created successfully");
    }
    
    // Check if graphical system is available
    static bool IsAvailable() {
        return gfx::IsAvailable();
    }
};

} } // namespace kos::ui

// Kernel interface function
extern "C" {
    void kos_show_process_monitor(void) {
        kos::ui::ProcessMonitorDemo::Run();
    }
}
