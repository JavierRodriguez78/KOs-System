#include <console/tty.hpp>
#include <drivers/vga.hpp>

using namespace kos::common;
using namespace kos::console;

    
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