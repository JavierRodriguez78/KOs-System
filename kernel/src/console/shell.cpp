
#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/libc.hpp>
#include <drivers/keyboard.hpp>
#include <fs/filesystem.hpp>
#include <lib/elfloader.hpp>

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
    ExecuteCommand(buffer);
}

void Shell::ExecuteCommand(const int8_t* command) {
    if (command == nullptr) return;
    if (*command == 0) return;

    // Check if command exists in /bin/
    int8_t path[64];
    int32_t cmdLen = LibC::strlen(command);
    // "/bin/" (5 chars) + command + null terminator must fit
    if (cmdLen + 5 + 1 > (int32_t)sizeof(path)) {
        tty.Write("Command too long\n");
        return;
    }
    // Build path: "/bin/" + command
    path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
    for (int32_t i = 0; i < cmdLen; ++i) {
        path[5 + i] = command[i];
    }
    path[5 + cmdLen] = 0;
    // Try to execute /bin/<cmd>.elf as ELF32
        int8_t elfPath[80];
        int32_t baseLen = LibC::strlen(path);
        if (baseLen + 4 < (int32_t)sizeof(elfPath)) {
            for (int32_t i = 0; i < baseLen; ++i) elfPath[i] = path[i];
            elfPath[baseLen] = '.'; 
            elfPath[baseLen+1] = 'e'; 
            elfPath[baseLen+2] = 'l'; 
            elfPath[baseLen+3] = 'f'; 
            elfPath[baseLen+4] = 0;
            if (g_fs_ptr) {
                static uint8_t elfBuf[64*1024]; // 64 KB buffer for small apps
                int32_t n = g_fs_ptr->ReadFile(elfPath, elfBuf, sizeof(elfBuf));
                if (n > 0) {
                    tty.Write((int8_t*)"Loading ELF...\n");
                    if (!kos::lib::ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n)) {
                        tty.Write("ELF load failed\n");
                    }
                    return;
                } else {
                    tty.Write((int8_t*)"ReadFile failed or empty\n");
                }
            }
        }
    tty.Write("Command not found: ");
    tty.Write(command);
    tty.PutChar('\n');
    
       
}

