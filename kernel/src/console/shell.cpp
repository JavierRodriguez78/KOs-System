
#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>
#include <drivers/keyboard.hpp>
#include <fs/filesystem.hpp>
#include <lib/elfloader.hpp>
// sys API utilities are declared in stdio.hpp
#include <lib/stdio.hpp>
// Logo printer
#include <console/logo.hpp>
#include <graphics/framebuffer.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::drivers;
using namespace kos::sys;

// File-local TTY instance for output
static TTY tty;

// Optional filesystem access from shell
namespace kos { 
    namespace fs { 
        class Filesystem; 
    } 
}

extern kos::fs::Filesystem* g_fs_ptr;

// (logo printer now lives in src/console/logo.cpp)

Shell::Shell() : bufferIndex(0) {
    for (int32_t i = 0; i < BUFFER_SIZE; ++i) buffer[i] = 0;
}

void Shell::PrintPrompt() {
    const int8_t* cwd = kos::sys::table()->cwd ? kos::sys::table()->cwd : (const int8_t*)"/";
    tty.Write(cwd);
    tty.Write((const int8_t*)"$ ");
}

void Shell::Run() {
 
    if (kos::gfx::IsAvailable()) {
        PrintLogoFramebuffer32();
    } else {
        PrintLogoBlockArt();
    }
    // Initialize current working directory to /home (create if missing)
    if (g_fs_ptr) {
        // Best-effort ensure /HOME exists (8.3 uppercase) and select a valid CWD.
        // If creation fails or directory still absent, fall back to root.
        g_fs_ptr->Mkdir((const int8_t*)"/HOME", 1);
        if (g_fs_ptr->DirExists((const int8_t*)"/HOME")) {
            SetCwd((const int8_t*)"/home");
        } else {
            SetCwd((const int8_t*)"/");
        }
    } else {
        // No filesystem mounted; use root as a neutral prompt path
        SetCwd((const int8_t*)"/");
    }
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

        // Parse program name and arguments from command line without mutating original buffer
        // Make a local mutable copy to split tokens
        int8_t localBuf[BUFFER_SIZE];
        int iCopy = 0;
        for (; command[iCopy] && iCopy < BUFFER_SIZE - 1; ++iCopy) localBuf[iCopy] = command[iCopy];
        localBuf[iCopy] = 0;

        const int32_t MAX_ARGS = 16;
        const int8_t* argv[MAX_ARGS];
        int32_t argc = 0;
        int8_t* p = localBuf;
        while (*p == ' ') ++p;
        while (*p && argc < MAX_ARGS) {
            argv[argc++] = p;
            while (*p && *p != ' ') ++p;
            if (!*p) break;
            *p++ = 0; // terminate token
            while (*p == ' ') ++p; // skip spaces before next token
    }
    if (argc == 0) return;
    const int8_t* prog = argv[0];


    // Built-in: show logo (text mode)
    if (String::strcmp(prog, (const int8_t*)"logo", 4) == 0 &&
        (prog[4] == 0)) {
        PrintLogoBlockArt();
        return;
    }

    // Built-in: show logo on framebuffer (32bpp)
    if (String::strcmp(prog, (const int8_t*)"logo32", 6) == 0 &&
        (prog[6] == 0)) {
        if (gfx::IsAvailable()) {
            PrintLogoFramebuffer32();
        } else {
            tty.Write("Framebuffer 32bpp not available, falling back to text logo\n");
            PrintLogoBlockArt();
        }
        return;
    }

    // Build /bin/<prog>
    int8_t path[64];
    int32_t cmdLen = String::strlen(prog);
    // "/bin/" (5 chars) + command + null terminator must fit
    if (cmdLen + 5 + 1 > (int32_t)sizeof(path)) {
        tty.Write("Command too long\n");
        return;
    }
    // Build path: "/bin/" + command
    path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
    // Safe copy of program name into path buffer
    String::memmove(path + 5, prog, (uint32_t)cmdLen);
    path[5 + cmdLen] = 0;
    // Try to execute /bin/<cmd>.elf as ELF32
    int8_t elfPath[80];
    int32_t baseLen = String::strlen(path);
    if (baseLen + 4 < (int32_t)sizeof(elfPath)) {
        // Copy base path into elfPath
        String::memmove(elfPath, path, (uint32_t)baseLen);
        elfPath[baseLen] = '.';
        elfPath[baseLen+1] = 'e';
            elfPath[baseLen+2] = 'l'; 
            elfPath[baseLen+3] = 'f'; 
            elfPath[baseLen+4] = 0;
            if (g_fs_ptr) {
                static uint8_t elfBuf[256*1024]; // 256 KB buffer for apps
                int32_t n = g_fs_ptr->ReadFile(elfPath, elfBuf, sizeof(elfBuf));
                if (n > 0) {
                    tty.Write((int8_t*)"Loading ELF ");
                    tty.Write(elfPath);
                    tty.Write((int8_t*)" size=");
                    tty.WriteHex((n >> 8) & 0xFF);
                    tty.WriteHex(n & 0xFF);
                    tty.Write((int8_t*)"...\n");
                    // Set args into system API for the app to read
                    SetArgs(argc, argv, command);
                    if (!ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n)) {
                        tty.Write("ELF load failed\n");
                    }
                    return;
                } else {
                    tty.Write((int8_t*)"ReadFile failed or empty\n");
                }
            }
        }
    tty.Write("Command not found: ");
    tty.Write(prog);
    tty.PutChar('\n');
    
       
}

