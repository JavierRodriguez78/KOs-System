#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <console/tty.hpp>
#include <drivers/keyboard/keyboard.hpp>
#include <lib/stdio.hpp>


using namespace kos::common;    
using namespace kos::console;



ShellKeyboardHandler::ShellKeyboardHandler(){
};

ShellKeyboardHandler::~ShellKeyboardHandler(){

};

// Reference to the shell instances
extern Shell* g_shell;

void ShellKeyboardHandler::OnKeyDown(int8_t c){
    // First, see if stdio scanf is actively reading input
    if (kos::sys::TryDeliverKey(c)) {
        return; // consumed by scanf/input reader
    }
    // Priority: threaded shell if available, otherwise fallback to original shell
    if (kos::console::g_threaded_shell) {
        kos::console::g_threaded_shell->OnKeyPressed(c);
    } else if (g_shell) {
        g_shell->InputChar(c);
    }
}