//View https://wiki.osdev.org/PS/2_Keyboard

#include "keyboard.hpp"

KeyboardDriver::KeyboardDriver(InterruptManager* manager)
:InterruptHandler(manager, 0x21),
dataport(0x60),
commandport(0x64)
{
    while(commandport.Read() & 0x1)
        dataport.Read();
    commandport.Write(0xAE); //activate interrupts
    commandport.Write(0x20); //get current state -> command 0x20 = read controller command byte
    uint8_t status = (dataport.Read() | 1) & ~0x10;
    commandport.Write(0x60); //set state -> command 0x60 = set controller command byte
    dataport.Write(status);
    dataport.Write(0xF4); //Activate keyboard

};
KeyboardDriver::~KeyboardDriver()
{

};

void printf(char*);
  
uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp)
{
    uint8_t key = dataport.Read();
    static bool Shift = false;
    if(key < 0x80)
    {
        switch(key)
        {
            // Numeric Keys
            case 0x02: if(Shift) {printf ("!"); Shift=false;}  else printf("1"); break;
            case 0x03: if(Shift) {printf ("\""); Shift=false;} else printf("2"); break;
            case 0x04: if(Shift) {printf ("·"); Shift=false;}  else printf("3"); break;
            case 0x05: if(Shift) {printf ("$"); Shift=false;}  else printf("4"); break;
            case 0x06: if(Shift) {printf ("%"); Shift=false;}  else printf("5"); break;
            case 0x07: if(Shift) {printf ("&"); Shift=false;}  else printf("6"); break;
            case 0x08: if(Shift) {printf ("/"); Shift=false;}  else printf("7"); break;
            case 0x09: if(Shift) {printf ("("); Shift=false;}  else printf("8"); break;
            case 0x0A: if(Shift) {printf (")"); Shift=false;}  else printf("9"); break;
            case 0x0B: if(Shift) {printf ("="); Shift=false;}  else printf("0"); break;

            case 0x10: if(Shift) {printf ("Q"); Shift=false;}  else printf("q"); break;
            case 0x11: if(Shift) {printf ("W"); Shift=false;}  else printf("w"); break;
            case 0x12: if(Shift) {printf ("E"); Shift=false;}  else printf("e"); break;
            case 0x13: if(Shift) {printf ("R"); Shift=false;}  else printf("r"); break;
            case 0x14: if(Shift) {printf ("T"); Shift=false;}  else printf("t"); break;
            case 0x15: if(Shift) {printf ("Z"); Shift=false;}  else printf("z"); break;
            case 0x16: if(Shift) {printf ("U"); Shift=false;}  else printf("u"); break;
            case 0x17: if(Shift) {printf ("I"); Shift=false;}  else printf("i"); break;
            case 0x18: if(Shift) {printf ("O"); Shift=false;}  else printf("o"); break;
            case 0x19: if(Shift) {printf ("P"); Shift=false;}  else printf("p"); break;

            case 0x1E: if(Shift) {printf ("A"); Shift=false;}  else printf("a"); break;
            case 0x1F: if(Shift) {printf ("S"); Shift=false;}  else printf("s"); break;
            case 0x20: if(Shift) {printf ("D"); Shift=false;}  else printf("d"); break;
            case 0x21: if(Shift) {printf ("F"); Shift=false;}  else printf("f"); break;
            case 0x22: if(Shift) {printf ("G"); Shift=false;}  else printf("g"); break;
            case 0x23: if(Shift) {printf ("H"); Shift=false;}  else printf("h"); break;
            case 0x24: if(Shift) {printf ("J"); Shift=false;}  else printf("j"); break;
            case 0x25: if(Shift) {printf ("K"); Shift=false;}  else printf("k"); break;
            case 0x26: if(Shift) {printf ("L"); Shift=false;}  else printf("l"); break;

            case 0x2C: if(Shift) {printf ("Y"); Shift=false;}  else printf("y"); break;
            case 0x2D: if(Shift) {printf ("X"); Shift=false;}  else printf("x"); break;
            case 0x2E: if(Shift) {printf ("C"); Shift=false;}  else printf("c"); break;
            case 0x2F: if(Shift) {printf ("V"); Shift=false;}  else printf("v"); break;
            case 0x30: if(Shift) {printf ("B"); Shift=false;}  else printf("b"); break;
            case 0x31: if(Shift) {printf ("N"); Shift=false;}  else printf("n"); break;
            case 0x32: if(Shift) {printf ("M"); Shift=false;}  else printf("m"); break;
            case 0x33: if(Shift) {printf (";"); Shift=false;}  else printf(","); break;
            case 0x34: if(Shift) {printf (":"); Shift=false;}  else printf("."); break;
            case 0x35: if(Shift) {printf ("_"); Shift=false;}  else printf("-"); break;

            case 0x1C: printf("\n"); break;
            case 0x39: printf(" "); break;
            case 0x2A: case 0x36 : Shift = true; break;

            //ignored
            case 0x45: //NumLock
                break;
            
            default:
                if(key<0x80)
                {
                    char* foo = "KEYBOARD 0x00";
                    char* hex = "0123456789ABCDEF";
                    foo[11] = hex[(key >> 4) & 0xF];
                    foo[12] = hex[key & 0xF];
                    printf(foo);
                }
                break;
        }
    }
      
    return esp;
};