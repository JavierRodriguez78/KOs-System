#include <drivers/vga.hpp>

using namespace kos::drivers;
using namespace kos::common;

    kos::common::uint16_t* VGA::VideoMemory = (kos::common::uint16_t*)0xb8000;
    kos::common::uint8_t VGA::x = 0;
    kos::common::uint8_t VGA::y = 0;

    VGA::VGA(){
       
    }

    VGA::~VGA(){

    }

    void VGA::Init(){
        Clear();
    }

    void VGA::Clear(){
    
        for(uint32_t i=0; i<80*25; i++){
            VGA::VideoMemory[i] = (0x07 << 8) | ' ';
        }
        VGA::x=0;
        VGA::y=0;
    }

    void VGA::PutChar(kos::common::int8_t c){
        if(c =='\n'){
            VGA::x=0; 
            VGA::y++;
        }else{
            VGA::VideoMemory[VGA::y * 80+VGA::x] = (0x07 << 8) | c;
            VGA::x++;
            if (VGA::x>=80) { 
                VGA::x=0; 
                VGA::y++;
            }
        }
        if (VGA::y >= 25) {
             Clear();
        }
    }

    void VGA::Write(const kos::common::int8_t* str){
        for (kos::common::uint32_t i = 0; str[i] !=0; i++)
            PutChar(str[i]);
        
    }

     void VGA::WriteHex(kos::common::uint8_t key){
        kos::common::int8_t foo[] = "00";
        const kos::common::int8_t* hex = "0123456789ABCDEF";
        foo[0] = hex[(key >> 4) & 0xF];
        foo[1] = hex[key & 0xF];
        Write(foo);
     }