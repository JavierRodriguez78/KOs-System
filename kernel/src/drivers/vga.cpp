#include <drivers/vga.hpp>

using namespace kos::drivers;
using namespace kos::common;

    uint16_t* VGA::VideoMemory = (uint16_t*)0xb8000;
    uint8_t VGA::x = 0;
    uint8_t VGA::y = 0;
    uint8_t VGA::attr = 0x07; // light grey on black

    VGA::VGA(){
       
    }

    VGA::~VGA(){

    }

    void VGA::Init(){
        Clear();
    }

    void VGA::Clear(){
    
        for(uint32_t i=0; i<80*25; i++){
            VGA::VideoMemory[i] = ((uint16_t)attr << 8) | ' ';
        }
        VGA::x=0;
        VGA::y=0;
    }

    void VGA::PutChar(kos::common::int8_t c){
        if(c =='\n'){
            VGA::x=0; 
            VGA::y++;
        }else{
            VGA::VideoMemory[VGA::y * 80+VGA::x] = ((uint16_t)attr << 8) | (uint8_t)c;
            VGA::x++;
            if (VGA::x>=80) { 
                VGA::x=0; 
                VGA::y++;
            }
        }
        if (VGA::y >= 25) {
            // Scroll up by one line: move rows 1..24 to rows 0..23
            for (uint32_t row = 1; row < 25; ++row) {
                for (uint32_t col = 0; col < 80; ++col) {
                    VideoMemory[(row-1)*80 + col] = VideoMemory[row*80 + col];
                }
            }
            // Clear last line
            for (uint32_t col = 0; col < 80; ++col) {
                VideoMemory[24*80 + col] = ((uint16_t)attr << 8) | ' ';
            }
            VGA::y = 24;
        }
    }

    void VGA::Write(const int8_t* str){
        for (uint32_t i = 0; str[i] !=0; i++)
            PutChar(str[i]);
        
    }

     void VGA::WriteHex(uint8_t key){
        int8_t foo[] = "00";
        const int8_t* hex = "0123456789ABCDEF";
        foo[0] = hex[(key >> 4) & 0xF];
        foo[1] = hex[key & 0xF];
        Write(foo);
     }

    void VGA::SetColor(uint8_t fg, uint8_t bg){
        attr = MakeAttr(fg, bg);
    }

    void VGA::SetAttr(uint8_t a){
        attr = a;
    }