#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <console/tty.hpp>
#include <drivers/keyboard.hpp>


using namespace kos::common;    
using namespace kos::console;



ShellKeyboardHandler::ShellKeyboardHandler(){
};

ShellKeyboardHandler::~ShellKeyboardHandler(){

};

// Reference to the shell instances
extern Shell* g_shell;

void ShellKeyboardHandler::OnKeyDown(int8_t c){
    // Priority: threaded shell if available, otherwise fallback to original shell
    if (kos::console::g_threaded_shell) {
        kos::console::g_threaded_shell->OnKeyPressed(c);
    } else if (g_shell) {
        g_shell->InputChar(c);
    }
}