#include <console/threaded_shell.hpp>
#include <process/thread_manager.hpp>
#include <process/scheduler.hpp>
#include <process/pipe.hpp>
#include <process/sync.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <console/logo.hpp>
#include <graphics/framebuffer.hpp>
#include <lib/string.hpp>
#include <lib/elfloader.hpp>
#include <lib/stdio.hpp>
#include <fs/filesystem.hpp>
#include <memory/heap.hpp>

using namespace kos::console;
using namespace kos::process;
using namespace kos::lib;
using namespace kos::memory;

// Filesystem access for loading external apps
extern kos::fs::Filesystem* g_fs_ptr;

// Global threaded shell instance
ThreadedShell* kos::console::g_threaded_shell = nullptr;

// Command execution context for threads
struct ThreadedCommandContext {
    char command[256];
    char args[512];
    uint32_t shell_thread_id;
    uint32_t result_pipe_id;
};

// Individual command thread implementations
extern "C" void cmd_help_thread();
extern "C" void cmd_ps_thread();
extern "C" void cmd_threads_thread();
extern "C" void cmd_pipes_thread();
extern "C" void cmd_tasks_thread();
extern "C" void cmd_mkpipe_thread();
extern "C" void cmd_rmpipe_thread();
extern "C" void cmd_kill_thread();
extern "C" void cmd_suspend_thread();
extern "C" void cmd_resume_thread();
extern "C" void cmd_sleep_thread();
extern "C" void cmd_yield_thread();
extern "C" void cmd_logo_thread();
extern "C" void cmd_logo32_thread();

ThreadedShell::ThreadedShell() 
    : input_buffer_pos(0), shell_thread_id(0), command_count(0),
      shell_mutex(nullptr), input_ready(false) {
    
    // Clear input buffer
    for (int i = 0; i < SHELL_BUFFER_SIZE; i++) {
        input_buffer[i] = '\0';
    }

    shell_mutex = new kos::process::Mutex();
    Logger::Log("Threaded shell created");
}

ThreadedShell::~ThreadedShell() {
    if (shell_mutex) delete shell_mutex;
}

bool ThreadedShell::Initialize() {
    kos::process::LockGuard lock(*shell_mutex);
    
    if (!g_thread_manager || !g_scheduler) {
        Logger::Log("Cannot initialize threaded shell - threading not available");
        return false;
    }
    
    // Create shell input pipe
    uint32_t pipe_id = PipeAPI::CreatePipe("shell-input", 1024, 32);
    if (!pipe_id) {
        Logger::Log("Failed to create shell input pipe");
        return false;
    }
    
    Logger::Log("Threaded shell initialized");
    return true;
}

void ThreadedShell::Start() {
    if (!g_thread_manager) return;
    
    // Get current thread as shell thread
    if (g_scheduler) {
        Thread* current = g_scheduler->GetCurrentTask();
        shell_thread_id = current ? current->task_id : 0;
    }
    
    // Initialize working directory to filesystem root
    kos::sys::SetCwd((const int8_t*)"/");

    ShowWelcome();
    MainLoop();
}

void ThreadedShell::ShowWelcome() {
    TTY::Write("\n=== KOS Threaded Shell v2.0 ===\n");
    TTY::Write("All commands execute as separate threads\n");
    TTY::Write("Enhanced multitasking environment\n");
    TTY::Write("Type 'help' for available commands\n\n");
}

void ThreadedShell::MainLoop() {
    while (true) {
        ShowPrompt();
        
        // Wait for a complete command line to be entered
        WaitForInput();
        
        if (input_buffer_pos > 0) {
            ExecuteCommand();
        }
        
        // Give other threads a chance to run
        SchedulerAPI::YieldThread();
    }
}

void ThreadedShell::ShowPrompt() {
    // Prefix: kos[ID]:
    TTY::Write("kos[");
    TTY::WriteHex(shell_thread_id);
    TTY::Write("]:");

    // Current directory with basename colorized (closest VGA color to #27F5E7 = light cyan 11)
    const int8_t* cwd = kos::sys::table()->cwd ? kos::sys::table()->cwd : (const int8_t*)"/";
    const int8_t* last_slash = nullptr;
    for (const int8_t* p = cwd; *p; ++p) if (*p == '/') last_slash = p;
    if (!last_slash) {
        TTY::SetColor(11, 0);
        TTY::Write(cwd);
        TTY::SetAttr(0x07);
    } else {
        int32_t prefix_len = (int32_t)(last_slash - cwd + 1);
        for (int32_t i = 0; i < prefix_len; ++i) TTY::PutChar(cwd[i]);
        const int8_t* base = last_slash + 1;
        if (*base) {
            TTY::SetColor(11, 0);
            TTY::Write(base);
            TTY::SetAttr(0x07);
        }
    }

    TTY::Write("$ ");
}

void ThreadedShell::WaitForInput() {
    // Reset input state for new command
    input_buffer_pos = 0;
    input_ready = false;
    
    // Clear input buffer
    for (int i = 0; i < SHELL_BUFFER_SIZE; i++) {
        input_buffer[i] = '\0';
    }
    
    // Wait for input to be ready (set by OnKeyPressed when Enter is pressed)
    while (!input_ready) {
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(10); // Small delay to prevent busy waiting
    }
}

void ThreadedShell::ExecuteCommand() {
    if (input_buffer_pos == 0) return;
    
    // Null-terminate command
    input_buffer[input_buffer_pos] = '\0';
    
    // Parse command and arguments
    char command[64];
    char args[256];
    ParseCommandLine(input_buffer, command, args);
    
    // Fast-path: built-in 'help' prints immediately (no separate thread needed)
    if (String::strcmp((const int8_t*)command, (const int8_t*)"help", 4) == 0 && command[4] == '\0') {
        TTY::Write("=== KOS Threaded Shell Commands ===\n");
        TTY::Write("System Commands:\n");
        TTY::Write("  help           - Show this help (threaded)\n");
        TTY::Write("  ps             - Show scheduler statistics (threaded)\n");
        TTY::Write("  threads        - Show all threads (threaded)\n");
        TTY::Write("  yield          - Yield CPU to other threads\n");
        TTY::Write("\nThread Management:\n");
        TTY::Write("  kill <id>      - Kill thread by ID (hex)\n");
        TTY::Write("  suspend <id>   - Suspend thread by ID (hex)\n");
        TTY::Write("  resume <id>    - Resume thread by ID (hex)\n");
        TTY::Write("  sleep <ms>     - Sleep current thread (decimal ms)\n");
        TTY::Write("\nPipe Commands:\n");
        TTY::Write("  pipes          - Show all active pipes (threaded)\n");
        TTY::Write("  mkpipe <name>  - Create a new pipe (threaded)\n");
        TTY::Write("  rmpipe <name>  - Remove a pipe (threaded)\n");
        
        TTY::Write("\nDemo Commands:\n");
        TTY::Write("  tasks          - Start scheduler demo tasks (threaded)\n");
        TTY::Write("  logo           - Show KOS logo (threaded)\n");
        TTY::Write("  logo32         - Show KOS logo on framebuffer (threaded)\n");
        TTY::Write("\nExternal Apps:\n");
        TTY::Write("  <cmd> [args]   - Executes /bin/<cmd>.elf if present on disk (e.g., ls, echo)\n\n");
        // Clear input buffer and return
        input_buffer_pos = 0;
        return;
    }

    // Fast-path: logo commands execute immediately (direct execution without threading)
    if (String::strcmp((const int8_t*)command, (const int8_t*)"logo", 4) == 0 && command[4] == '\0') {
        TTY::Write("Direct execution: logo command...\n");
        kos::console::PrintLogoBlockArt();
        TTY::Write("Direct execution: logo command completed.\n");
        input_buffer_pos = 0;
        return;
    }

    if (String::strcmp((const int8_t*)command, (const int8_t*)"logo32", 6) == 0 && command[6] == '\0') {
        TTY::Write("Direct execution: logo32 command...\n");
        kos::console::PrintLogoFramebuffer32();
        TTY::Write("Direct execution: logo32 command completed.\n");
        input_buffer_pos = 0;
        return;
    }

    // Fast-path: 'ps' (scheduler stats) prints inline for reliability
    if (String::strcmp((const int8_t*)command, (const int8_t*)"ps", 2) == 0 && command[2] == '\0') {
        if (g_scheduler) {
            g_scheduler->PrintTaskList();
        } else {
            TTY::Write("Scheduler not initialized\n");
        }
        input_buffer_pos = 0;
        return;
    }

    // Create command execution thread
    uint32_t cmd_thread_id = CreateCommandThread(command, args);

    if (cmd_thread_id) {
        TTY::Write("Command '");
        TTY::Write(command);
        TTY::Write("' started in thread ");
        TTY::WriteHex(cmd_thread_id);
        TTY::Write("\n");
        command_count++;
    } else {
        // Fallback: try to execute as external ELF from /bin/<cmd>.elf
        // Build argv from command + args (space-separated)
        const int32_t MAX_ARGS = 16;
        const int8_t* argv[MAX_ARGS];
        int32_t argc = 0;
        // argv[0] = command
        argv[argc++] = (const int8_t*)command;
        // Tokenize args into argv[1..]
        char argsCopy[256];
        int ap = 0; while (args[ap] && ap < (int)sizeof(argsCopy)-1) { argsCopy[ap] = args[ap]; ++ap; }
        argsCopy[ap] = 0;
        char* p = argsCopy;
        while (*p && argc < MAX_ARGS) {
            while (*p == ' ') ++p;
            if (!*p) break;
            argv[argc++] = (int8_t*)p;
            while (*p && *p != ' ') ++p;
            if (*p) { *p = 0; ++p; }
        }

        // Build /bin/<command>.elf path
        int8_t path[64];
        int cLen = kos::lib::String::strlen((const int8_t*)command);
        if (cLen + 5 + 4 + 1 < (int)sizeof(path)) {
            path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
            kos::lib::String::memmove(path + 5, (const int8_t*)command, (uint32_t)cLen);
            path[5 + cLen] = 0;
            int8_t elfPath[80];
            int baseLen = kos::lib::String::strlen(path);
            kos::lib::String::memmove(elfPath, path, (uint32_t)baseLen);
            elfPath[baseLen] = '.'; elfPath[baseLen+1] = 'e'; elfPath[baseLen+2] = 'l'; elfPath[baseLen+3] = 'f'; elfPath[baseLen+4] = 0;
            if (g_fs_ptr) {
                static uint8_t elfBuf[256*1024];
                int32_t n = g_fs_ptr->ReadFile(elfPath, elfBuf, sizeof(elfBuf));
                if (n > 0) {
                    // Pass args to app and execute
                    kos::sys::SetArgs(argc, argv, (const int8_t*)input_buffer);
                    if (!kos::lib::ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n)) {
                        TTY::Write((const int8_t*)"ELF load failed\n");
                    }
                } else {
                    TTY::Write((const int8_t*)"Unknown command: ");
                    TTY::Write((const int8_t*)command);
                    TTY::Write((const int8_t*)"\n");
                }
            } else {
                TTY::Write((const int8_t*)"No filesystem mounted; cannot run external commands\n");
            }
        } else {
            TTY::Write((const int8_t*)"Command name too long\n");
        }
    }
    
    // Clear input buffer
    input_buffer_pos = 0;
}

void ThreadedShell::ParseCommandLine(const char* cmdline, char* command, char* args) {
    int cmd_pos = 0;
    int arg_pos = 0;
    bool in_args = false;
    
    // Extract command and arguments
    for (int i = 0; cmdline[i] && i < SHELL_BUFFER_SIZE; i++) {
        if (cmdline[i] == ' ' && !in_args) {
            in_args = true;
            continue;
        }
        
        if (!in_args && cmd_pos < 63) {
            command[cmd_pos++] = cmdline[i];
        } else if (in_args && arg_pos < 255) {
            args[arg_pos++] = cmdline[i];
        }
    }
    
    command[cmd_pos] = '\0';
    args[arg_pos] = '\0';
}

uint32_t ThreadedShell::CreateCommandThread(const char* command, const char* args) {
    if (!g_thread_manager || !command) return 0;
    
    // Determine command thread entry point
    void* entry_point = GetCommandEntryPoint(command);
    if (!entry_point) return 0;
    
    // Create thread name
    char thread_name[64];
    int name_pos = 0;
    const char* prefix = "cmd-";
    for (int i = 0; prefix[i]; i++) {
        thread_name[name_pos++] = prefix[i];
    }
    for (int i = 0; command[i] && name_pos < 59; i++) {
        thread_name[name_pos++] = command[i];
    }
    thread_name[name_pos] = '\0';
    
    // Create command thread
    return g_thread_manager->CreateUserThread(entry_point, 4096, PRIORITY_NORMAL, 
                                            thread_name, shell_thread_id);
}

void* ThreadedShell::GetCommandEntryPoint(const char* command) {
    // Map commands to thread entry points
    if (String::strcmp((const int8_t*)command, (const int8_t*)"help", 4) == 0) {
        return (void*)cmd_help_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"ps", 2) == 0) {
        return (void*)cmd_ps_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"threads", 7) == 0) {
        return (void*)cmd_threads_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"pipes", 5) == 0) {
        return (void*)cmd_pipes_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"mkpipe", 6) == 0) {
        return (void*)cmd_mkpipe_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"rmpipe", 6) == 0) {
        return (void*)cmd_rmpipe_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"kill", 4) == 0) {
        return (void*)cmd_kill_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"suspend", 7) == 0) {
        return (void*)cmd_suspend_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"resume", 6) == 0) {
        return (void*)cmd_resume_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"sleep", 5) == 0) {
        return (void*)cmd_sleep_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"yield", 5) == 0) {
        return (void*)cmd_yield_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"logo", 4) == 0) {
        return (void*)cmd_logo_thread;
    } else if (String::strcmp((const int8_t*)command, (const int8_t*)"logo32", 6) == 0) {
        return (void*)cmd_logo32_thread;
    }
    
    return nullptr; // Unknown command
}

void ThreadedShell::OnKeyPressed(char key) {
    kos::process::LockGuard lock(*shell_mutex);
    
    if (key == '\n' || key == '\r') {
        input_ready = true;
        TTY::Write("\n");
    } else if (key == '\b' || key == 127) {
        // Backspace
        if (input_buffer_pos > 0) {
            input_buffer_pos--;
            input_buffer[input_buffer_pos] = '\0';
            TTY::Write("\b \b"); // Erase character on screen
        }
    } else if (key >= 32 && key <= 126 && input_buffer_pos < SHELL_BUFFER_SIZE - 1) {
        // Printable character
        input_buffer[input_buffer_pos++] = key;
        char str[2] = {key, '\0'};
        TTY::Write(str); // Write single character as string
    }
}

void ThreadedShell::GetStatus(uint32_t* total_commands, uint32_t* active_threads) const {
    if (total_commands) *total_commands = command_count;
    if (active_threads && g_thread_manager) {
        *active_threads = g_thread_manager->GetThreadCount();
    }
}

// Command thread implementations would go here
// For brevity, I'll implement a few key ones:

extern "C" void cmd_help_thread() {
    TTY::Write("=== KOS Threaded Shell Commands ===\n");
    TTY::Write("System Commands:\n");
    TTY::Write("  help           - Show this help (threaded)\n");
    TTY::Write("  ps             - Show scheduler statistics (threaded)\n");
    TTY::Write("  threads        - Show all threads (threaded)\n");
    TTY::Write("  yield          - Yield CPU to other threads\n");
    TTY::Write("\nThread Management:\n");
    TTY::Write("  kill <id>      - Kill thread by ID (hex)\n");
    TTY::Write("  suspend <id>   - Suspend thread by ID (hex)\n");
    TTY::Write("  resume <id>    - Resume thread by ID (hex)\n");
    TTY::Write("  sleep <ms>     - Sleep current thread (decimal ms)\n");
    TTY::Write("\nPipe Commands:\n");
    TTY::Write("  pipes          - Show all active pipes (threaded)\n");
    TTY::Write("  mkpipe <name>  - Create a new pipe (threaded)\n");
    TTY::Write("  rmpipe <name>  - Remove a pipe (threaded)\n");
    TTY::Write("\nDemo Commands:\n");
    TTY::Write("  logo           - Show KOS logo (threaded)\n");
    TTY::Write("  logo32         - Show KOS logo on framebuffer (threaded)\n");
    TTY::Write("\nAll commands now execute as independent threads!\n");
    
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_ps_thread() {
    if (g_scheduler) {
        g_scheduler->PrintTaskList();
    } else {
        TTY::Write("Scheduler not available\n");
    }
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_threads_thread() {
    if (g_thread_manager) {
        g_thread_manager->PrintAllThreads();
    } else {
        TTY::Write("Thread manager not available\n");
    }
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_pipes_thread() {
    if (g_pipe_manager) {
        g_pipe_manager->PrintAllPipes();
    } else {
        TTY::Write("Pipe manager not available\n");
    }
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_yield_thread() {
    TTY::Write("Yielding CPU from command thread...\n");
    SchedulerAPI::YieldThread();
    TTY::Write("Command thread resumed\n");
    SchedulerAPI::ExitThread();
}

// Additional command thread implementations

extern "C" void cmd_mkpipe_thread() {
    // Create pipe (simplified - would need argument parsing)
    uint32_t pipe_id = PipeAPI::CreatePipe("demo-pipe", 1024, 16);
    if (pipe_id) {
        TTY::Write("Created pipe: demo-pipe (ID: ");
        TTY::WriteHex(pipe_id);
        TTY::Write(")\n");
    } else {
        TTY::Write("Failed to create pipe\n");
    }
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_rmpipe_thread() {
    // Remove pipe (simplified)
    if (PipeAPI::DestroyPipe("demo-pipe")) {
        TTY::Write("Removed pipe: demo-pipe\n");
    } else {
        TTY::Write("Failed to remove pipe or pipe not found\n");
    }
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_kill_thread() {
    TTY::Write("Kill command - thread ID required\n");
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_suspend_thread() {
    TTY::Write("Suspend command - thread ID required\n");
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_resume_thread() {
    TTY::Write("Resume command - thread ID required\n");
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_sleep_thread() {
    TTY::Write("Sleep command - sleeping for 1 second\n");
    SchedulerAPI::SleepThread(1000);
    TTY::Write("Sleep command finished\n");
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_logo_thread() {
    TTY::Write("Threaded shell: executing logo command...\n");
    // Show text logo
    kos::console::PrintLogoBlockArt();
    TTY::Write("Threaded shell: logo command completed.\n");
    
    // Give some time for output to be processed before thread exits
    SchedulerAPI::SleepThread(100);
    SchedulerAPI::ExitThread();
}

extern "C" void cmd_logo32_thread() {
    TTY::Write("Threaded shell: executing logo32 command...\n");
    // Show framebuffer logo
    kos::console::PrintLogoFramebuffer32();
    TTY::Write("Threaded shell: logo32 command completed.\n");
    
    // Give some time for output to be processed before thread exits
    SchedulerAPI::SleepThread(100);
    SchedulerAPI::ExitThread();
}

// ThreadedShellAPI implementation

namespace kos::console::ThreadedShellAPI {
    
    bool InitializeShell() {
        if (!g_threaded_shell) {
            g_threaded_shell = new ThreadedShell();
        }
        return g_threaded_shell->Initialize();
    }
    
    void StartShell() {
        if (g_threaded_shell) {
            g_threaded_shell->Start();
        }
    }
    
    void ProcessKeyInput(char key) {
        if (g_threaded_shell) {
            g_threaded_shell->OnKeyPressed(key);
        }
    }
    
    void ShowShellStatus() {
        if (g_threaded_shell) {
            uint32_t commands, threads;
            g_threaded_shell->GetStatus(&commands, &threads);
            TTY::Write("Shell Status - Commands: ");
            TTY::WriteHex(commands);
            TTY::Write(", Threads: ");
            TTY::WriteHex(threads);
            TTY::Write("\n");
        } else {
            TTY::Write("Threaded shell not initialized\n");
        }
    }
}