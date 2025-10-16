
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
// Scheduler / threads
#include <process/scheduler.hpp>
// Pipe management
#include <process/pipe.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::drivers;
using namespace kos::sys;
using namespace kos::process;

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
    // Prompt prefix: kos[00]:
    tty.Write((const int8_t*)"kos[");
    tty.Write((const int8_t*)"00");
    tty.Write((const int8_t*)"]:");

    // Print path with basename colorized (closest VGA color to #27F5E7 is light cyan = 11)
    // Find last '/'
    const int8_t* last_slash = nullptr;
    for (const int8_t* p = cwd; *p; ++p) {
        if (*p == '/') last_slash = p;
    }
    if (!last_slash) {
        // No slash found, colorize entire cwd
        tty.SetColor(11, 0);
        tty.Write(cwd);
        tty.SetAttr(0x07);
    } else {
        // Write prefix up to and including last '/'
        // Compute length from cwd start to last_slash inclusive
        int32_t prefix_len = (int32_t)(last_slash - cwd + 1);
        // Write prefix normally
        for (int32_t i = 0; i < prefix_len; ++i) tty.PutChar(cwd[i]);
        // Write basename in color
        const int8_t* base = last_slash + 1;
        if (*base) {
            tty.SetColor(11, 0);
            tty.Write(base);
            tty.SetAttr(0x07);
        }
    }

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
        tty.Write("Executing logo command...\n");
        PrintLogoBlockArt();
        tty.Write("Logo command completed.\n");
        return;
    }

    // Built-in: show logo on framebuffer (32bpp)
    if (String::strcmp(prog, (const int8_t*)"logo32", 6) == 0 &&
        (prog[6] == 0)) {
        tty.Write("Executing logo32 command...\n");
        if (gfx::IsAvailable()) {
            tty.Write("Framebuffer available, showing 32bpp logo.\n");
            PrintLogoFramebuffer32();
        } else {
            tty.Write("Framebuffer 32bpp not available, falling back to text logo\n");
            PrintLogoBlockArt();
        }
        tty.Write("Logo32 command completed.\n");
        return;
    }



    // Built-in: show scheduler statistics
    if (String::strcmp(prog, (const int8_t*)"ps", 2) == 0 &&
        (prog[2] == 0)) {
        if (!g_scheduler) {
            tty.Write("Scheduler not initialized\n");
        } else {
            tty.Write("Scheduler Statistics:\n");
            tty.Write("  Current task ID: ");
            tty.WriteHex(SchedulerAPI::GetCurrentThreadId());
            tty.Write("\n");

            tty.Write("  Total tasks: ");
            tty.WriteHex(g_scheduler->GetTaskCount());
            tty.Write("\n");

            tty.Write("  Scheduling enabled: ");
            tty.Write(g_scheduler->IsSchedulingEnabled() ? "Yes" : "No");
            tty.Write("\n");
        }
        return;
    }

    // Built-in: detailed thread list
    if (String::strcmp(prog, (const int8_t*)"threads", 7) == 0 &&
        (prog[7] == 0)) {
        if (g_scheduler) {
            g_scheduler->PrintTaskList();
        } else {
            tty.Write("Scheduler not initialized\n");
        }
        return;
    }

    // Built-in: kill thread
    if (String::strcmp(prog, (const int8_t*)"kill", 4) == 0 &&
        (prog[4] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }
        
        if (SchedulerAPI::KillThread(thread_id)) {
            tty.Write("Thread killed successfully\n");
        } else {
            tty.Write("Failed to kill thread\n");
        }
        return;
    }

    // Built-in: suspend thread
    if (String::strcmp(prog, (const int8_t*)"suspend", 7) == 0 &&
        (prog[7] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }
        
        if (SchedulerAPI::SuspendThread(thread_id)) {
            tty.Write("Thread suspended successfully\n");
        } else {
            tty.Write("Failed to suspend thread\n");
        }
        return;
    }

    // Built-in: resume thread
    if (String::strcmp(prog, (const int8_t*)"resume", 6) == 0 &&
        (prog[6] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }

        if (SchedulerAPI::ResumeThread(thread_id)) {
            tty.Write("Thread resumed successfully\n");
        } else {
            tty.Write("Failed to resume thread\n");
        }
        return;
    }

    // Built-in: sleep current thread
    if (String::strcmp(prog, (const int8_t*)"sleep", 5) == 0 &&
        (prog[5] == 0) && argc >= 2) {
        uint32_t ms = 0;
        // Simple decimal parsing for milliseconds
        const int8_t* ms_str = argv[1];
        for (int i = 0; ms_str[i]; i++) {
            if (ms_str[i] >= '0' && ms_str[i] <= '9') {
                ms = ms * 10 + (ms_str[i] - '0');
            }
        }
        
        tty.Write("Sleeping for ");
        tty.WriteHex(ms);
        tty.Write(" ms...\n");
        SchedulerAPI::SleepThread(ms);
        return;
    }

    // Built-in: yield current task (if any)
    if (String::strcmp(prog, (const int8_t*)"yield", 5) == 0 &&
        (prog[5] == 0)) {
        tty.Write("Yielding CPU...\n");
        SchedulerAPI::YieldThread();
        return;
    }

    // Built-in: pipes command (show all pipes)
    if (String::strcmp(prog, (const int8_t*)"pipes", 5) == 0 &&
        (prog[5] == 0)) {
        if (g_pipe_manager) {
            g_pipe_manager->PrintAllPipes();
        } else {
            tty.Write("Pipe manager not initialized\n");
        }
        return;
    }

    // Built-in: mkpipe command (create pipe)
    if (String::strcmp(prog, (const int8_t*)"mkpipe", 6) == 0 &&
        (prog[6] == ' ' || prog[6] == 0)) {
        if (prog[6] == 0) {
            tty.Write("Usage: mkpipe <name> [buffer_size]\n");
            return;
        }
        
        // Find pipe name
        const int8_t* name_start = prog + 7;
        while (*name_start == ' ') name_start++; // Skip spaces
        
        if (*name_start == 0) {
            tty.Write("Usage: mkpipe <name> [buffer_size]\n");
            return;
        }
        
        // Find end of name
        const int8_t* name_end = name_start;
        while (*name_end && *name_end != ' ') name_end++;
        
        // Extract name
        char pipe_name[32];
        int name_len = name_end - name_start;
        if (name_len >= sizeof(pipe_name)) name_len = sizeof(pipe_name) - 1;
        for (int i = 0; i < name_len; i++) {
            pipe_name[i] = name_start[i];
        }
        pipe_name[name_len] = 0;
        
        uint32_t pipe_id = PipeAPI::CreatePipe(pipe_name);
        if (pipe_id) {
            tty.Write("Created pipe: ");
            tty.Write(pipe_name);
            tty.Write(" (ID: ");
            tty.WriteHex(pipe_id);
            tty.Write(")\n");
        } else {
            tty.Write("Failed to create pipe\n");
        }
        return;
    }

    // Built-in: rmpipe command (remove pipe)
    if (String::strcmp(prog, (const int8_t*)"rmpipe", 6) == 0 &&
        (prog[6] == ' ' || prog[6] == 0)) {
        if (prog[6] == 0) {
            tty.Write("Usage: rmpipe <name>\n");
            return;
        }
        
        // Find pipe name
        const int8_t* name_start = prog + 7;
        while (*name_start == ' ') name_start++; // Skip spaces
        
        if (*name_start == 0) {
            tty.Write("Usage: rmpipe <name>\n");
            return;
        }
        
        // Find end of name
        const int8_t* name_end = name_start;
        while (*name_end && *name_end != ' ') name_end++;
        
        // Extract name
        char pipe_name[32];
        int name_len = name_end - name_start;
        if (name_len >= sizeof(pipe_name)) name_len = sizeof(pipe_name) - 1;
        for (int i = 0; i < name_len; i++) {
            pipe_name[i] = name_start[i];
        }
        pipe_name[name_len] = 0;
        
        if (PipeAPI::DestroyPipe(pipe_name)) {
            tty.Write("Removed pipe: ");
            tty.Write(pipe_name);
            tty.Write("\n");
        } else {
            tty.Write("Failed to remove pipe or pipe not found\n");
        }
        return;
    }

    // Built-in: help command
    if (String::strcmp(prog, (const int8_t*)"help", 4) == 0 &&
        (prog[4] == 0)) {
        tty.Write("KOS Shell Commands:\n");
        tty.Write("  help           - Show this help message\n");
        tty.Write("  logo           - Show KOS logo in text mode\n");
        tty.Write("  logo32         - Show KOS logo on framebuffer\n");
        
        
        tty.Write("  ps             - Show scheduler statistics\n");
        tty.Write("  threads        - Show detailed thread list\n");
        tty.Write("  yield          - Yield CPU to other tasks\n");
        tty.Write("  kill <id>      - Kill thread by ID (hex)\n");
        tty.Write("  suspend <id>   - Suspend thread by ID (hex)\n");
        tty.Write("  resume <id>    - Resume thread by ID (hex)\n");
        tty.Write("  sleep <ms>     - Sleep current thread (decimal ms)\n");
        tty.Write("  pipes          - Show all active pipes\n");
        tty.Write("  mkpipe <name>  - Create a new pipe\n");
        tty.Write("  rmpipe <name>  - Remove a pipe\n");
        tty.Write("  <cmd>          - Execute /bin/<cmd>.elf from filesystem\n");
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

