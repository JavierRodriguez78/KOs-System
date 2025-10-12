#include <drivers/vga.hpp>
#include <drivers/vga_constants.hpp>

using namespace kos::drivers;
using namespace kos::common;

    uint16_t* VGA::VideoMemory = (uint16_t*)VGA_TEXT_BUFFER;
    uint8_t VGA::x = 0;
    uint8_t VGA::y = 0;
    uint8_t VGA::attr = VGA_COLOR_DEFAULT;  

    VGA::VGA(){
       
    }

    VGA::~VGA(){

    }

    void VGA::Init(){
        Clear();
    }

    void VGA::Clear(){

        uint16_t blank = ((uint16_t)attr << 8) | VGA_BLANK_CHAR;
        // Fill entire screen with blank characters
        for(uint32_t i=0; i<VGA_WIDTH*VGA_HEIGHT; i++){
            VideoMemory[i] = blank;
        }
        x=0;
        y=0;
    }

    void VGA::PutChar(int8_t c){
        if(c =='\n'){
            x=0; 
            y++;
        }else{
            VideoMemory[y * 80+x] = ((uint16_t)attr << 8) | (uint8_t)c;
            x++;
            if (x>=VGA_WIDTH) { 
                x=0; 
                y++;
            }
        }
        if (y >= VGA_HEIGHT) {
            // Scroll up by one line: move rows 1..24 to rows 0..23
            for (uint32_t row = 1; row < VGA_HEIGHT; ++row) {
                for (uint32_t col = 0; col < VGA_WIDTH; ++col) {
                    VideoMemory[(row-1)*VGA_WIDTH + col] = VideoMemory[row*VGA_WIDTH + col];
                }
            }
            // Clear last line
            uint16_t blank = ((uint16_t)attr << 8) | ' ';
            for (uint32_t col = 0; col < VGA_WIDTH; ++col) {
                VideoMemory[(VGA_HEIGHT-1)*VGA_WIDTH + col] = blank;
            }
            y = VGA_HEIGHT-1;
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