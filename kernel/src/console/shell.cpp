
#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/libc.hpp>
#include <drivers/keyboard.hpp>
#include <fs/filesystem.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::drivers;

// File-local TTY instance for output
static TTY tty;

// Optional filesystem access from shell
namespace kos { 
    namespace fs { 
        class Filesystem; 
    } 
}

extern kos::fs::Filesystem* g_fs_ptr;

Shell::Shell() : bufferIndex(0) {
    for (int32_t i = 0; i < BUFFER_SIZE; ++i) buffer[i] = 0;
}

void Shell::PrintPrompt() {
    tty.Write("$ ");
}

void Shell::Run() {
    tty.Write("Welcome to KOS Shell\n");
    PrintPrompt();
    while (true) {
        // In a real kernel, input would come from keyboard interrupts
        // Here, input is handled via InputChar() called by the keyboard handler
    }
}

void Shell::InputChar(int8_t c) {
    if (c == '\n' || c == '\r') {
        tty.PutChar('\n');
        buffer[bufferIndex] = 0;
        ExecuteCommand();
        bufferIndex = 0;
        PrintPrompt();
    } else if (c == '\b' || c == 127) { // Backspace
        if (bufferIndex > 0) {
            bufferIndex--;
            tty.Write("\b \b");
        }
    } else if (bufferIndex < BUFFER_SIZE - 1) {
        buffer[bufferIndex++] = c;
        tty.PutChar(c);
    }
}

void Shell::ExecuteCommand() {
    if (bufferIndex == 0) return;
    if (LibC::strcmp(reinterpret_cast<const uint8_t*>(buffer), (const uint8_t*)"help") == 0) {
        tty.Write("Commands: help, clear, ls, fatinfo\n");
    } else if (LibC::strcmp(reinterpret_cast<const uint8_t*>(buffer), (const uint8_t*)"clear") == 0) {
        //tty.Clear();
    } else if (LibC::strcmp(reinterpret_cast<const uint8_t*>(buffer), (const uint8_t*)"ls") == 0) {
        if (g_fs_ptr) {
            g_fs_ptr->ListRoot();
        } else {
            tty.Write("No filesystem mounted\n");
        }
    } else if (LibC::strcmp(reinterpret_cast<const uint8_t*>(buffer), (const uint8_t*)"fatinfo") == 0) {
        if (g_fs_ptr) {
            g_fs_ptr->DebugInfo();
        } else {
            tty.Write("No filesystem mounted\n");
        }
    } else {
        tty.Write("Unknown command\n");
    }
}

