#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/libc.hpp>
#include <drivers/keyboard.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::drivers;

Shell::Shell(){

}

Shell::~Shell(){

}

//Basic command Shell
void Shell::Exec(const kos::common::uint8_t* cmd){
    const kos::common::uint8_t* HELP = reinterpret_cast<const kos::common::uint8_t*>("help");
    if (LIBC.strcmp(cmd, HELP) == 0) {
        TTY.Write("Comands: \n");
    }
}

void Shell::Run(){
    TTY.Write("- Wellcome KOS'Shell \n");
    while(true){
        
    }
}

