#include <common/types.hpp>
#include <gdt.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/pci.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard.hpp>
#include <drivers/mouse.hpp>

using namespace kos;
using namespace kos::common;
using namespace kos::drivers;
using namespace kos::hardware;

void printf(char* str)
{
    //Sección de punto de memoria donde escribir en pantalla.
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    static uint8_t x=0, y=0;

    for (int i=0; str[i] !='\0'; ++i)
    {

        switch (str[i])
        {
            case '\n' :
                y++;
                x=0;
                break;
            default:        
                VideoMemory[80*y+x]  =(VideoMemory[80*y+x] & 0xFF00) | str[i];
                x++;
                break;;
        }

        if(x>=80)
        {
            y++;
            x=0;
        }
        if(y>=25)
        {
            for(y=0; y<25; y++)
                for(x=0; y<80; x++)
                 VideoMemory[80*y+x]  =(VideoMemory[80*y+x] & 0xFF00) | ' ';

            x=0;
            y=0;
     
        }
    }
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
};


class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char* foo = " ";
        foo[0] = c;
        printf(foo);
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
  
    printf("Hello World!!!!! KOs\n");
    printf("Operating System ---\n");

    GlobalDescriptorTable gdt;
    InterruptManager interrupts(0x20, &gdt);

    printf("Initializing Hardware, Stage 1\n");
    DriverManager drvManager;

        PrintfKeyboardEventHandler kbhandler;
        KeyboardDriver keyboard(&interrupts, &kbhandler);
        drvManager.AddDriver(&keyboard);

        MouseToConsole mhandler;
        MouseDriver mouse(&interrupts, &mhandler);
        drvManager.AddDriver(&mouse);

        PeripheralComponentIntercontroller PCIController;
        PCIController.SelectDrivers(&drvManager);

        printf("Initializing Hardware, Stage 2\n");
        drvManager.ActivateAll();

    printf("Initializing Hardware, Stage 3\n");
    interrupts.Activate();
    while(1);
}