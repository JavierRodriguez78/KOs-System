#include <common/types.hpp>
#include <memory/gdt.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/pci.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard.hpp>
#include <drivers/mouse.hpp>
#include <drivers/vga.hpp>
#include <console/tty.hpp>
#include <console/shell.hpp>
#include <drivers/ata.hpp>
#include <fs/fat16.hpp>
#include <fs/fat32.hpp>
#include <fs/filesystem.hpp>


using namespace kos;
using namespace kos::common;
using namespace kos::drivers;
using namespace kos::hardware;
using namespace kos::console;
using namespace kos::fs;


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
static kos::fs::FAT16 g_fs16_p0m(&g_ata_p0m);
static kos::fs::FAT16 g_fs16_p0s(&g_ata_p0s);
static kos::fs::FAT16 g_fs16_p1m(&g_ata_p1m);
static kos::fs::FAT16 g_fs16_p1s(&g_ata_p1s);

Filesystem* g_fs_ptr = 0;
// Global shell instance
Shell g_shell_instance;

extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    // Call all constructors
    callConstructors();

    static kos::console::TTY tty;
    //tty.Clear();
    tty.Write((kos::common::int8_t*)"---- KOS Kernel 64-bit ---- \n");
    tty.Write((kos::common::int8_t*)"--- Operating System ---\n");

    GlobalDescriptorTable gdt;
    InterruptManager interrupts(0x20, &gdt);
    // Register a noop handler for IRQ14 (IDE primary) to avoid log spam if device still raises IRQs
    IDEIRQHandler ideIrq14(&interrupts, interrupts.HardwareInterruptOffset() + 14);

    tty.Write("--- Initializing Hardware, Stage 1 ---\n");
    DriverManager drvManager;

    tty.Write("--- LOAD DEVICES ---\n");
    ShellKeyboardHandler skbhandler;
    KeyboardDriver keyboard(&interrupts, &skbhandler);
    drvManager.AddDriver(&keyboard);

    // MouseToConsole mhandler;
    // MouseDriver mouse(&interrupts, &mhandler);
    // drvManager.AddDriver(&mouse);

    PeripheralComponentIntercontroller PCIController;
    PCIController.SelectDrivers(&drvManager);

    tty.Write ("--- Initializing Hardware, Stage 2 ---\n");
    drvManager.ActivateAll();

    tty.Write ("--- Initializing Hardware, Stage 3 ---\n");
    interrupts.Activate();

    // Try to mount FAT32 on any IDE position (Primary/Secondary x Master/Slave)
    struct Candidate { ATADriver* ata; FAT32* fs32; FAT16* fs16; const char* name; };
    Candidate cands[] = {
        { &g_ata_p0m, &g_fs32_p0m, &g_fs16_p0m, "Primary Master" },
        { &g_ata_p0s, &g_fs32_p0s, &g_fs16_p0s, "Primary Slave" },
        { &g_ata_p1m, &g_fs32_p1m, &g_fs16_p1m, "Secondary Master" },
        { &g_ata_p1s, &g_fs32_p1s, &g_fs16_p1s, "Secondary Slave" }
    };

    tty.Write("--- Scanning ATA devices for FAT32 ---\n");
    for (unsigned i = 0; i < sizeof(cands)/sizeof(cands[0]); ++i) {
        cands[i].ata->Activate();
        tty.Write("Probe: "); 
        tty.Write((int8_t*)cands[i].name); 
        tty.PutChar('\n');
        if (!cands[i].ata->Identify()) {
            tty.Write("No ATA device detected at "); 
            tty.Write((int8_t*)cands[i].name); 
            tty.PutChar('\n');
            continue;
        }
        if (cands[i].fs32->Mount()) {
            tty.Write("FAT32 mounted on "); 
            tty.Write((int8_t*)cands[i].name); 
            tty.PutChar('\n');
            g_fs_ptr = cands[i].fs32;
            tty.Write("Root entries:\n");
            g_fs_ptr->ListRoot();
            break;
        } else {
            tty.Write("Not FAT32 at "); 
            tty.Write((int8_t*)cands[i].name); 
            tty.PutChar('\n');
            
            // Try FAT16 mount as fallback (common on old images)
            if (cands[i].fs16->Mount()) {
                tty.Write("FAT16 mounted on "); 
                tty.Write((int8_t*)cands[i].name); 
                tty.PutChar('\n');
                g_fs_ptr = cands[i].fs16;
                tty.Write("Root entries:\n");
                g_fs_ptr->ListRoot();
                break;
            }
        }
    }
    if (!g_fs_ptr) {
        tty.Write("FAT32 mount failed on all IDE positions\n");
    }

    // Create and start the shell
    g_shell = &g_shell_instance;
    g_shell_instance.Run();

    while(1);
}