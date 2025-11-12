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

// Global mouse driver pointer for fallback polling (used by WindowManager)
kos::drivers::mouse::MouseDriver* g_mouse_driver_ptr = nullptr;


class MouseToConsole: public MouseEventHandler
{
    int8_t x,y;
public:

    MouseToConsole()
    {
    };

    virtual void OnActivate()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
            
    }

};

// Mouse handler that routes events into the UI input system when graphics is available
class MouseToUI : public MouseEventHandler {
public:
    virtual void OnActivate() override {}
    virtual void OnMouseMove(int xoffset, int yoffset) override {
        if (kos::gfx::IsAvailable()) { kos::ui::MouseMove(xoffset, yoffset); }
    }
    virtual void OnMouseDown(uint8_t button) override {
        if (kos::gfx::IsAvailable()) { kos::ui::MouseButtonDown(button); }
    }
    virtual void OnMouseUp(uint8_t button) override {
        if (kos::gfx::IsAvailable()) { kos::ui::MouseButtonUp(button); }
    }
};
static MouseToUI g_mouse_ui_handler;

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



typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)(); 
}




// Global shell pointer for keyboard handler
kos::console::Shell* g_shell = nullptr;

// Global shell instance
Shell g_shell_instance;

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

static BannerService g_banner_service;
static TimeService g_time_service;
static kos::services::WindowManager g_window_manager;
static FilesystemService g_fs_service;
static kos::services::InitDService g_initd_service;

extern "C" void kernelMain(const void* multiboot_structure, uint32_t multiboot_magic)
{
    // Initialize framebuffer info (if booted via Multiboot2 with framebuffer tag)
    InitFromMultiboot(multiboot_structure, multiboot_magic);
    // Call all constructors
    callConstructors();

    static TTY tty;
    tty.Clear();

    // If a 32bpp framebuffer is available (set by GRUB), clear to a neutral dark wallpaper color.
    // This avoids the previous mint-green flash on boot and matches the compositor's wallpaper.
    if (kos::gfx::IsAvailable()) {
        Logger::Log("Framebuffer (32bpp) detected; initializing graphics background to dark gray");
        // ARGB 0xFF1E1E20
        kos::gfx::Clear32(0xFF1E1E20u);
    }

    // Parse Multiboot cmdline for debug and mouse settings (supports MB1 and MB2)
    // Minimal multiboot v1 info structure (we only need flags and cmdline)
    struct MultibootInfoMinimal { uint32_t flags; uint32_t mem_lower; uint32_t mem_upper; uint32_t boot_device; uint32_t cmdline; };
    auto contains = [](const char* hay, const char* needle) -> bool {
        if (!hay || !needle) return false;
        uint32_t nlen = String::strlen((const int8_t*)needle);
        for (uint32_t i = 0; hay[i]; ++i) {
            // Compare at this position for nlen
            if (String::strcmp((const int8_t*)(hay + i), (const int8_t*)needle, nlen) == 0) return true;
        }
        return false;
    };
    uint8_t mousePollMode = 2; // default always
    // Multiboot v1: EAX=0x2BADB002, EBX=mb1 info
    if (multiboot_magic == 0x2BADB002 && multiboot_structure) {
        const MultibootInfoMinimal* mbi = (const MultibootInfoMinimal*)multiboot_structure;
        // flags bit 2 (1<<2) indicates cmdline present in multiboot v1
        if (mbi->flags & (1u << 2)) {
            const char* cmd = (const char*)mbi->cmdline;
            if (contains(cmd, "debug") || contains(cmd, "log=debug")) {
                Logger::SetDebugEnabled(true);
                // Print a one-time info message so users know it's active
                Logger::Log("Debug mode enabled via boot param");
            }
            if (contains(cmd, "panic=reboot") || contains(cmd, "reboot_on_panic")) {
                kos::kernel::SetPanicReboot(true);
                Logger::Log("Panic: reboot-on-panic enabled via boot param");
            }
            // Parse mouse_poll=never|once|always
            auto opt = cmd;
            while (opt && *opt) {
                if (*opt == 'm' && String::strcmp((const int8_t*)opt, (const int8_t*)"mouse_poll=", 11) == 0) {
                    const char* val = opt + 11;
                    if (String::strcmp((const int8_t*)val, (const int8_t*)"never", 5) == 0) mousePollMode = 0;
                    else if (String::strcmp((const int8_t*)val, (const int8_t*)"once", 4) == 0) mousePollMode = 1;
                    else if (String::strcmp((const int8_t*)val, (const int8_t*)"always", 6) == 0) mousePollMode = 2;
                }
                ++opt;
            }
            switch (mousePollMode) {
                case 0: Logger::Log("Boot param: mouse_poll=never"); break;
                case 1: Logger::Log("Boot param: mouse_poll=once"); break;
                case 2: Logger::Log("Boot param: mouse_poll=always"); break;
            }
        }
    } else if (multiboot_magic == 0x36d76289 && multiboot_structure) {
        // Multiboot v2: EAX=0x36d76289, EBX=mb2 info; parse cmdline tag (type=1)
        const uint8_t* p = (const uint8_t*)multiboot_structure;
        // uint32_t total_size = *(const uint32_t*)p; (void)total_size;
        p += 8; // skip total_size and reserved
        while (true) {
            struct Tag { uint32_t type; uint32_t size; };
            const Tag* tag = (const Tag*)p;
            if (tag->type == 0) break; // end
            if (tag->type == 1) {
                const char* cmd = (const char*)(p + sizeof(Tag));
                if (contains(cmd, "debug") || contains(cmd, "log=debug")) {
                    Logger::SetDebugEnabled(true);
                    Logger::Log("Debug mode enabled via boot param (MB2)");
                }
                if (contains(cmd, "panic=reboot") || contains(cmd, "reboot_on_panic")) {
                    kos::kernel::SetPanicReboot(true);
                    Logger::Log("Panic: reboot-on-panic enabled via boot param (MB2)");
                }
                // Parse mouse_poll=never|once|always
                auto opt = cmd;
                while (opt && *opt) {
                    if (*opt == 'm' && String::strcmp((const int8_t*)opt, (const int8_t*)"mouse_poll=", 11) == 0) {
                        const char* val = opt + 11;
                        if (String::strcmp((const int8_t*)val, (const int8_t*)"never", 5) == 0) mousePollMode = 0;
                        else if (String::strcmp((const int8_t*)val, (const int8_t*)"once", 4) == 0) mousePollMode = 1;
                        else if (String::strcmp((const int8_t*)val, (const int8_t*)"always", 6) == 0) mousePollMode = 2;
                    }
                    ++opt;
                }
                switch (mousePollMode) {
                    case 0: Logger::Log("Boot param (MB2): mouse_poll=never"); break;
                    case 1: Logger::Log("Boot param (MB2): mouse_poll=once"); break;
                    case 2: Logger::Log("Boot param (MB2): mouse_poll=always"); break;
                }
                // Only one cmdline expected; stop after parsing
                break;
            }
            // advance to next tag (8-byte aligned)
            uint32_t size = tag->size;
            p += (size + 7) & ~7u;
        }
    }

    // Do not forcibly disable debug after parsing cmdline; default is off unless enabled above
    Logger::Log("KOS Kernel starting...");
    Logger::LogStatus("Initializing core subsystems", true);

    GlobalDescriptorTable gdt;
    // Initialize system API table for apps
    InitSysApi();
    InterruptManager interrupts(0x20, &gdt);
    // Register a noop handler for IRQ14 (IDE primary) to avoid log spam if device still raises IRQs
    IDEIRQHandler ideIrq14(&interrupts, interrupts.HardwareInterruptOffset() + 14);
    Logger::LogStatus("IDT/GDT configured", true);
    // Initialize Physical Memory Manager (using Multiboot v1 basic fields if available)
    extern uint8_t kernel_start; extern uint8_t kernel_end;
    uint32_t memLowerKB = 0, memUpperKB = 0;
    if (multiboot_magic == 0x2BADB002 && multiboot_structure) {
        const MultibootInfoMinimal* mbi = (const MultibootInfoMinimal*)multiboot_structure;
        if (mbi->flags & 1) { memLowerKB = mbi->mem_lower; memUpperKB = mbi->mem_upper; }
    }
    PMM::Init(memLowerKB, memUpperKB, (phys_addr_t)&kernel_start, (phys_addr_t)&kernel_end, multiboot_structure);
    Logger::LogStatus("PMM initialized", true);

    // Enable paging
    Paging::Init((phys_addr_t)&kernel_start, (phys_addr_t)&kernel_end);
    Logger::LogStatus("Paging enabled", true);

    // Initialize a tiny kernel heap away from app load base (apps link at 0x01000000)
    // Use 0x02000000 (32 MiB) with a couple of pages; still inside 64 MiB identity window
    Heap::Init((virt_addr_t)0x02000000, 2);
    Logger::LogStatus("Kernel heap initialized", true);

    // Initialize scheduler and timer for preemptive multitasking
    g_scheduler = new Scheduler();
    SchedulerTimerHandler* timer_handler = new SchedulerTimerHandler(&interrupts, g_scheduler);
    Logger::LogStatus("Scheduler initialized", true);

    // Initialize pipe manager for inter-task communication
    g_pipe_manager = new PipeManager();
    Logger::LogStatus("Pipe manager initialized", true);

    // Initialize thread manager for comprehensive threading
    if (ThreadManagerAPI::InitializeThreading()) {
        Logger::LogStatus("Thread manager initialized", true);
    } else {
        Logger::LogStatus("Thread manager initialization failed", false);
    }

    Logger::Log("Initializing Hardware, Stage 1");
    DriverManager drvManager;

    // Initialize shell pointer for keyboard handler fallback
    g_shell = &g_shell_instance;

    Logger::Log("Loading device drivers");
    ShellKeyboardHandler skbhandler;
    KeyboardDriver keyboard(&interrupts, &skbhandler);
    drvManager.AddDriver(&keyboard);

    // In graphics mode, route mouse events to UI input
    MouseDriver mouse(&interrupts, &g_mouse_ui_handler);
    // Expose pointer for fallback polling path invoked by WindowManager
    g_mouse_driver_ptr = &mouse;
    drvManager.AddDriver(&mouse);

    // Add scaffold NIC drivers; they will self-probe for matching PCI devices
    Rtl8139Driver nic_rtl8139;
    E1000Driver   nic_e1000;
    Rtl8169Driver nic_rtl8169;
    drvManager.AddDriver(&nic_rtl8139);
    drvManager.AddDriver(&nic_e1000);
    drvManager.AddDriver(&nic_rtl8169);

    PeripheralComponentIntercontroller PCIController;
    PCIController.SelectDrivers(&drvManager);
    Logger::Log("Initializing Hardware, Stage 2");
    drvManager.ActivateAll();
    Logger::LogStatus("Hardware initialized Stage 2", true);
    Logger::Log("Initializing Hardware, Stage 3");
    interrupts.Activate();
    Logger::LogStatus("Hardware initialized Stage 3", true);
    
    // Explicitly enable timer interrupt (IRQ0) and keyboard interrupt (IRQ1)
    interrupts.EnableIRQ(0);
    Logger::LogStatus("Timer IRQ0 enabled", true);
    interrupts.EnableIRQ(1);
    Logger::LogStatus("Keyboard IRQ1 enabled", true);
    // Enable mouse IRQ (PS/2 auxiliary device)
    interrupts.EnableIRQ(12);
    Logger::LogStatus("Mouse IRQ12 enabled", true);
    
    // Enable preemptive scheduling now that interrupts are active
    g_scheduler->EnablePreemption();
    Logger::LogStatus("Preemptive scheduling enabled", true);

    // Try to mount FAT32 on any IDE position (Primary/Secondary x Master/Slave)
    struct Candidate { ATADriver* ata; FAT32* fs32; FAT16* fs16; const char* name; };
    Candidate cands[] = {
        { &g_ata_p0m, &g_fs32_p0m, &g_fs16_p0m, "Primary Master" },
        { &g_ata_p0s, &g_fs32_p0s, &g_fs16_p0s, "Primary Slave" },
        { &g_ata_p1m, &g_fs32_p1m, &g_fs16_p1m, "Secondary Master" },
        { &g_ata_p1s, &g_fs32_p1s, &g_fs16_p1s, "Secondary Slave" }
    };

    Logger::Log("Scanning ATA devices for filesystems");
    for (unsigned i = 0; i < sizeof(cands)/sizeof(cands[0]); ++i) {
        cands[i].ata->Activate();
        Logger::LogKV("Probe", cands[i].name);
        if (!cands[i].ata->Identify()) {
            Logger::LogKV("No ATA device detected at", cands[i].name);
            continue;
        }
        if (cands[i].fs32->Mount()) {
            Logger::LogKV("FAT32 mounted on", cands[i].name);
            Logger::LogStatus("Filesystem ready", true);
            g_fs_ptr = cands[i].fs32;
            break;
        } else {
            Logger::LogKV("Not FAT32 at", cands[i].name);
            Logger::LogStatus("FAT32 mount failed", false);
            
            // Try FAT16 mount as fallback (common on old images)
            if (cands[i].fs16->Mount()) {
                Logger::LogKV("FAT16 mounted on", cands[i].name);
                Logger::LogStatus("Filesystem ready", true);
                g_fs_ptr = cands[i].fs16;
                break;
            }
        }
    }
    if (!g_fs_ptr) {
        Logger::Log("No filesystem mounted; continuing without disk");
    }

    // Register built-in services and start them based on configuration
    // Filesystem must start early so other services relying on /ETC config can work
    ServiceManager::Register(&g_fs_service);
    ServiceManager::Register(&g_banner_service);
    ServiceManager::Register(&g_time_service);
    ServiceManager::Register(&g_window_manager);
    ServiceManager::Register(&g_initd_service);
    Logger::Log("Kernel: services registered (FS, BANNER, TIME, WINMAN, INITD)");
    // Apply mouse poll mode before WindowManager starts (service start will use static setter)
    WindowManager::SetMousePollMode(mousePollMode);
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

    // Built-in commands removed; rely on /bin/<cmd>.elf execution

    // Start the threading system and threaded shell
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

    // Main kernel thread continues to run
    while(1) {
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(1000); // Sleep 1 second
    }
}