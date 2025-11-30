// Moved from kernel.cpp: main kernel entry and initialization flow
#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard/keyboard.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/vga/vga.hpp>
#include <drivers/net/rtl8139/rtl8139.hpp>
#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/rtl8169/rtl8169.hpp>
#include <ui/input.hpp>
#include <console/tty.hpp>
#include <console/logger.hpp>
#include <console/shell.hpp>
#include <drivers/ata/ata.hpp>
#include <fs/fat16.hpp>
#include <fs/fat32.hpp>
#include <fs/filesystem.hpp>
#include <lib/stdio.hpp>
#include <graphics/framebuffer.hpp>
#include <kernel/init.hpp>
#include <kernel/drivers.hpp>
#include <kernel/fs.hpp>
#include <kernel/services.hpp>
#include <kernel/shell.hpp>
#include <kernel/globals.hpp>
#include <kernel/multiboot_kernel.hpp>
#include <kernel/boot_stage.hpp>
#include <services/window_manager.hpp>
#include <memory/memory.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>
#include <memory/heap.hpp>
#include <process/scheduler.hpp>
#include <process/pipe.hpp>
#include <process/thread_manager.hpp>
#include <process/timer.hpp>
#include <console/threaded_shell.hpp>
#include <services/service_manager.hpp>
#include <services/banner_service.hpp>
#include <services/time_service.hpp>
#include <services/filesystem_service.hpp>
#include <lib/elfloader.hpp>
#include <application/init/service.hpp>
#include <common/panic.hpp>


using namespace kos;
using namespace kos::common;
using namespace kos::drivers;
using namespace kos::drivers::ata;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::console;
using namespace kos::fs;
using namespace kos::memory;
using namespace kos::gfx;
using namespace kos::process;
using namespace kos::services;
using namespace kos::drivers::net::e1000;
using namespace kos::drivers::net::rtl8139;
using namespace kos::drivers::net::rtl8169;

#include <kernel/mouse_handlers.hpp>

// File-local constants replacing magic numbers for clarity
namespace {
    constexpr uint8_t kInterruptVectorBase = 0x20;          // Base vector offset for hardware interrupts
    constexpr uint8_t kIdePrimaryIrq = 14;                  // Primary IDE IRQ line number
    constexpr uint32_t kMainThreadSleepMs = 1000;           // Sleep duration for main kernel thread loop
    constexpr char kInitialShellPromptKey = '\n';          // Key injected to force initial prompt render
}

// Mouse handler that routes events into the UI input system when graphics is available

// Simple no-op handler for IDE primary IRQ (IRQ14 -> vector hardwareInterruptOffset+14)
class IDEIRQHandler : public InterruptHandler {
public:
    IDEIRQHandler(InterruptManager* im, uint8_t vector)
        : InterruptHandler(im, vector) {}
    virtual uint32_t HandleInterrupt(uint32_t esp) {
        // Do nothing; PIC EOI is handled centrally in InterruptManager
        return esp;
    }
};






// Note: shell globals are declared in `kernel/globals.hpp` and defined
// in `src/kernel_globals.cpp`. Do not redefine them here.

// Prepare ATA drivers for all 4 IDE positions and FAT32 instances for scanning
static ATADriver g_ata_p0m(ATADriver::Primary, ATADriver::Master);
static ATADriver g_ata_p0s(ATADriver::Primary, ATADriver::Slave);
static ATADriver g_ata_p1m(ATADriver::Secondary, ATADriver::Master);
static ATADriver g_ata_p1s(ATADriver::Secondary, ATADriver::Slave);

static FAT32 g_fs32_p0m(&g_ata_p0m);
static FAT32 g_fs32_p0s(&g_ata_p0s);
static FAT32 g_fs32_p1m(&g_ata_p1m);
static FAT32 g_fs32_p1s(&g_ata_p1s);

// Simple FAT16 instances with no pre-known startLBA (they will assume 0; our FAT16 mount handles volumeStartLBA passed in ctor)
static FAT16 g_fs16_p0m(&g_ata_p0m);
static FAT16 g_fs16_p0s(&g_ata_p0s);
static FAT16 g_fs16_p1m(&g_ata_p1m);
static FAT16 g_fs16_p1s(&g_ata_p1s);

namespace kos {
    namespace fs {
        extern Filesystem* g_fs_ptr;
    }
}

// Global service instances (file-scope to avoid thread-safe static guards)

// Stubs for C++ runtime functions required for static initialization and exit handlers
extern "C" int __cxa_guard_acquire(char* g) { return !*g; }
extern "C" void __cxa_guard_release(char* g) { *g = 1; }
extern "C" void __cxa_guard_abort(char*) {}
extern "C" int atexit(void (*)(void)) { return 0; }

// Service instances are defined in `kernel_services.cpp`.

extern "C" void kernelMain(const void* multiboot_structure, uint32_t multiboot_magic)
{
    using kos::kernel::BootStage; 
    using kos::kernel::BootProgressor;
    // Wire BootProgressor to a global uptime source (if available) so we
    // can capture per-stage timing for profiling. This remains optional:
    // on early boot where ServiceManager is not yet initialized, the
    // uptime may simply return 0.
    BootProgressor boot(&kos::services::ServiceManager::UptimeMs);
    boot.Advance(BootStage::EarlyInit);
    // Parse boot options early to set global selections (mouse poll, display mode)
    {
        kos::kernel::BootOptions opts = kos::kernel::BootOptions::ParseFromMultiboot(multiboot_structure, multiboot_magic);
        kos::g_mouse_poll_mode = opts.MousePollMode();
        kos::g_display_mode = opts.Mode();
    }

    // Initialize multiboot parsing and framebuffer info
    kos::kernel::MultibootKernel mb(multiboot_structure, multiboot_magic);
    mb.Init();

    static TTY tty;
    tty.Clear();

    // Multiboot parsing and framebuffer init handled by `MultibootKernel::Init()`
    // mousePollMode is provided by MultibootKernel if needed: uint8_t mousePollMode = mb.MousePollMode();
    Logger::Log("KOS Kernel starting...");
    Logger::LogStatus("Initializing core subsystems", true);

    GlobalDescriptorTable gdt;
    // Initialize system API table for apps
    InitSysApi();
    InterruptManager interrupts(kInterruptVectorBase, &gdt);
    // Register a noop handler for IRQ14 (IDE primary) to avoid log spam if device still raises IRQs
    IDEIRQHandler ideIrq14(&interrupts, interrupts.HardwareInterruptOffset() + kIdePrimaryIrq);
    Logger::LogStatus("IDT/GDT configured", true);
    // Initialize Physical Memory Manager (using Multiboot v1 basic fields if available)
    extern uint8_t kernel_start; extern uint8_t kernel_end;
    uint32_t memLowerKB = mb.MemLowerKB(), memUpperKB = mb.MemUpperKB();

    // Initialize core subsystems (PMM, paging, heap, scheduler, pipe manager, thread manager)
    kos::kernel::InitCore(&interrupts, memLowerKB, memUpperKB, &kernel_start, &kernel_end, multiboot_structure);
    boot.Advance(BootStage::MemoryInit);

    // Ensure kernel mouse handlers are constructed before we create the MouseDriver
    kos::kernel::InitKernelMouseHandlers();

    // Initialize hardware drivers and enable IRQs
    kos::kernel::InitDrivers(&interrupts);
    boot.Advance(BootStage::DriverInit);

    // Scan and mount filesystems (ATA -> FAT32/FAT16)
    kos::kernel::ScanAndMountFilesystems();
    boot.Advance(BootStage::FilesystemInit);

    // Prepare input subsystem early (before services) so cursor is ready when GUI windows appear.
    if (kos::g_display_mode == kos::kernel::DisplayMode::Graphics && kos::gfx::IsAvailable()) {
        kos::ui::InitInput();
        const auto& fbInfo = kos::gfx::GetInfo();
        kos::ui::SetCursorPos((int)(fbInfo.width/2), (int)(fbInfo.height/2));
        kos::ui::SetMouseSensitivity(2, 1);
        Logger::LogKV("Input", "initialized");
    }
    boot.Advance(BootStage::InputInit);

    // Register and start kernel services (filesystem, banner, time, window manager, initd)
    // Mouse poll mode is now supplied via global g_mouse_poll_mode
    kos::kernel::RegisterAndStartServices();
    boot.Advance(BootStage::ServicesInit);

    // Built-in commands removed; rely on /bin/<cmd>.elf execution

    // Start the threading system and threaded shell
    ThreadManagerAPI::StartMultitasking();
    Logger::LogStatus("Multitasking environment started", true);
    boot.Advance(BootStage::MultitaskingStart);
    
    // If graphics mode selected and available, mark graphics stage
    if (kos::g_display_mode == kos::kernel::DisplayMode::Graphics && kos::gfx::IsAvailable()) {
        boot.Advance(BootStage::GraphicsMode);
    }

    // Start user interaction depending on selected display mode
    if (kos::g_display_mode == kos::kernel::DisplayMode::Graphics && kos::gfx::IsAvailable()) {
        // Initialize and start threaded shell (graphics)
        if (ThreadedShellAPI::InitializeShell()) {
            Logger::LogStatus("Threaded shell initialized", true);
            ThreadedShellAPI::StartShell();
            ThreadedShellAPI::ProcessKeyInput(kInitialShellPromptKey);
            boot.Advance(BootStage::ShellInit);
        } else {
            Logger::LogStatus("Failed to initialize threaded shell", false);
            // Fallback to text shell if graphics shell fails
            g_shell = &g_shell_instance;
            Logger::LogStatus("Starting fallback text shell...", true);
            g_shell_instance.Run();
            boot.Advance(BootStage::ShellInit);
        }
    } else {
        // Text mode: start classic shell
        g_shell = &g_shell_instance;
        Logger::LogStatus("Starting text shell...", true);
        g_shell_instance.Run();
        boot.Advance(BootStage::ShellInit);
    }

    // Main kernel thread continues to run
    boot.Advance(BootStage::Complete);
    // Log a compact summary of per-stage timing once boot is complete.
    boot.LogTimingSummary();
    while(1) {
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(kMainThreadSleepMs); // Sleep 1 second
    }
}
