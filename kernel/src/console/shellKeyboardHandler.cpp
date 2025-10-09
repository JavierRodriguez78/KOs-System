#include <console/shell.hpp>
#include <console/tty.hpp>
#include <drivers/keyboard.hpp>


using namespace kos::common;    
using namespace kos::console;



ShellKeyboardHandler::ShellKeyboardHandler(){
};

ShellKeyboardHandler::~ShellKeyboardHandler(){

};

// Reference to the shell instance
extern Shell* g_shell;

void ShellKeyboardHandler::OnKeyDown(int8_t c){
    if (g_shell) {
        g_shell->InputChar(c);
    }
}