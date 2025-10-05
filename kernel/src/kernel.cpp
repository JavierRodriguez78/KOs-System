#include <common/types.hpp>
#include <gdt.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/pci.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard.hpp>
#include <drivers/mouse.hpp>
#include <drivers/vga.hpp>
#include <console/tty.hpp>

using namespace kos;
using namespace kos::common;
using namespace kos::drivers;
using namespace kos::hardware;
using namespace kos::console;



class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char foo[] = " ";
        foo[0] = c;
        static kos::console::TTY tty;
        tty.Write(foo);
    }
};

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




typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)(); 
}



extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{

    // Call all constructors
    callConstructors();

    kos::console::TTY tty;
    tty.Init();
    tty.Write((kos::common::int8_t*)"---- KOS Kernel 64-bit ---- \n");
    tty.Write((kos::common::int8_t*)"--- Operating System ---\n");

    GlobalDescriptorTable gdt;
    InterruptManager interrupts(0x20, &gdt);

    tty.Write("--- Initializing Hardware, Stage 1 ---\n");
    DriverManager drvManager;

    tty.Write("--- LOAD DEVICES ---\n");
   // PrintfKeyboardEventHandler kbhandler;
   // KeyboardDriver keyboard(&interrupts, &kbhandler);
   // drvManager.AddDriver(&keyboard);

    MouseToConsole mhandler;
    MouseDriver mouse(&interrupts, &mhandler);
    drvManager.AddDriver(&mouse);

    PeripheralComponentIntercontroller PCIController;
    PCIController.SelectDrivers(&drvManager);

    tty.Write ("--- Initializing Hardware, Stage 2 ---\n");
        drvManager.ActivateAll();

    tty.Write ("--- Initializing Hardware, Stage 3 ---\n");
    interrupts.Activate();
    while(1);
}