#include <console/tty.hpp>
#include <drivers/vga/vga.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers::vga;

    
    VGA TTY::vga;

    void TTY::Clear(){
        vga.Clear();
    }

    void TTY::PutChar(int8_t c){
        vga.PutChar(c);
    }

    void TTY::Write(const int8_t* str){
        vga.Write(str);
    }

    void TTY::WriteHex(uint8_t key){
        vga.WriteHex(key);
    }

    void TTY::SetColor(uint8_t fg, uint8_t bg){
        vga.SetColor(fg, bg);
    }

    void TTY::SetAttr(uint8_t a){
        vga.SetAttr(a);
    }