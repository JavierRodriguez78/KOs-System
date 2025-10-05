#include <console/tty.hpp>
#include <drivers/vga.hpp>

using namespace kos::drivers;
using namespace kos::console;


    TTY::TTY(){
    }

    
    TTY::~TTY(){
    }

    
    void TTY::Init(){
        vga.Init();
        
    }

    void TTY::Clear(){
        vga.Clear();
    }

    void TTY::PutChar(kos::common::int8_t c){
        vga.PutChar(c);
    }

    void TTY::Write(const kos::common::int8_t* str){
        vga.Write(str);
    }

    void TTY::WriteHex(kos::common::uint8_t key){
        vga.WriteHex(key);
    }