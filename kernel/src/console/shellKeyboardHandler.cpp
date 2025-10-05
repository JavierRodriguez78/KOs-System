#include <console/shell.hpp>
#include <console/tty.hpp>
#include <drivers/keyboard.hpp>


using namespace kos::console;
using namespace kos::drivers;


ShellKeyboardHandler::ShellKeyboardHandler(){
    tty.Init();
};

ShellKeyboardHandler::~ShellKeyboardHandler(){

};

void ShellKeyboardHandler::OnKeyDown(char c){
    char foo[] = " ";
    foo[0] = c;
    tty.Write(foo);
}