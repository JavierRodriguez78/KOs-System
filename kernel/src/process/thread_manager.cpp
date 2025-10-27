#include <process/thread_manager.hpp>
#include <process/scheduler.hpp>
#include <process/pipe.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <drivers/keyboard.hpp>
#include <lib/string.hpp>
#include <lib/elfloader.hpp>
#include <lib/sysapi.hpp>
#include <memory/heap.hpp>
#include <fs/filesystem.hpp>

using namespace kos::process;
using namespace kos::console;
using namespace kos::lib;
using namespace kos::memory;

// Global thread manager instance
ThreadManager* kos::process::g_thread_manager = nullptr;

// Command execution context
struct CommandContext {
    char command[256];
    uint32_t caller_thread_id;
};

// ThreadManager implementation

ThreadManager::ThreadManager() 
    : thread_registry(nullptr), thread_count(0), system_thread_count(0), 
      user_thread_count(0), main_thread(nullptr), shell_thread(nullptr),
    idle_thread(nullptr), threading_initialized(false) {
    
    manager_mutex = new Mutex();

    
        Logger::LogStatus("Thread manager created", true);
    

}

ThreadManager::~ThreadManager() {
    TerminateAllThreads();
    if (manager_mutex) delete manager_mutex;
}

bool ThreadManager::Initialize() {
    if (threading_initialized) return true;
    
    LockGuard lock(*manager_mutex);
    
    // Ensure scheduler is available
    if (!g_scheduler) {
        Logger::LogStatus("Cannot initialize threading - scheduler not available", false);
        return false;
    }
    
    // Create main kernel thread (convert current execution context)
    main_thread = ThreadFactory::CreateThread(1, (void*)kernel_main_thread, 8192, 
                                             PRIORITY_CRITICAL, "kernel-main");
    if (!main_thread) {
        Logger::LogStatus("Failed to create main kernel thread", false);    
        return false;
    }
    
    AddThreadEntry(main_thread, THREAD_KERNEL_MAIN, "Main kernel thread", 0, true);
    
    threading_initialized = true;
    Logger::LogStatus("Thread manager initialized", true);
    return true;
}

void ThreadManager::StartMultitasking() {
    if (!threading_initialized) {
        Logger::LogStatus("Threading not initialized", false);
        return;
    }

    Logger::LogStatus("Starting multitasking environment", true);
    CreateSystemThreads();
    
    // Enable preemptive scheduling
    if (g_scheduler) {
        g_scheduler->EnablePreemption();
        Logger::LogStatus("Preemptive multitasking enabled", true);
    }
}

void ThreadManager::CreateSystemThreads() {
    Logger::LogStatus("Creating system threads", true);
    
    // Create idle thread
    uint32_t idle_id = CreateSystemThread((void*)idle_thread, 4096, 
                                         PRIORITY_IDLE, THREAD_IDLE, "idle-thread", main_thread->task_id);
    if (idle_id) {
        idle_thread = g_scheduler->FindTask(idle_id);
            if (Logger::IsDebugEnabled()) {
                Logger::Log("Created idle thread");
            }
        
    }
    
    // Create shell thread
    uint32_t shell_id = CreateSystemThread((void*)shell_thread, 8192, 
                                          PRIORITY_HIGH, THREAD_SHELL, "shell-thread", main_thread->task_id);
    if (shell_id) {
        shell_thread = g_scheduler->FindTask(shell_id);
            if (Logger::IsDebugEnabled()) {
                Logger::Log("Created shell thread");
            }
    }
    
    // Create keyboard handler thread
    uint32_t keyboard_id = CreateSystemThread((void*)keyboard_thread, 4096,
                                             PRIORITY_HIGH, THREAD_KEYBOARD, "keyboard-thread", main_thread->task_id);
    if (keyboard_id) {
            if (Logger::IsDebugEnabled()) {
                Logger::Log("Created keyboard thread");
            }
        
    }
}

uint32_t ThreadManager::CreateSystemThread(void* entry_point, uint32_t stack_size,
                                          ThreadPriority priority, SystemThreadType type,
                                          const char* description, uint32_t parent_id) {
    if (!g_scheduler) return 0;
    
    Thread* thread = g_scheduler->CreateTask(entry_point, stack_size, priority, description);
    if (!thread) return 0;
    
    LockGuard lock(*manager_mutex);
    AddThreadEntry(thread, type, description, parent_id, true);
    if (Logger::IsDebugEnabled()) {
        Logger::Log("Created system thread");
    }
    return thread->task_id;
}

uint32_t ThreadManager::CreateUserThread(void* entry_point, uint32_t stack_size,
                                        ThreadPriority priority, const char* description,
                                        uint32_t parent_id) {
    if (!g_scheduler) return 0;
    
    Thread* thread = g_scheduler->CreateTask(entry_point, stack_size, priority, description);
    if (!thread) return 0;
    
    LockGuard lock(*manager_mutex);
    AddThreadEntry(thread, THREAD_USER_PROCESS, description, parent_id, false);
        if (Logger::IsDebugEnabled()) {
            Logger::Log("Created user thread");
        }
    
    return thread->task_id;
}

bool ThreadManager::TerminateThread(uint32_t thread_id) {
    if (!g_scheduler) return false;
    
    LockGuard lock(*manager_mutex);
    RemoveThreadEntry(thread_id);
    
    bool result = g_scheduler->KillTask(thread_id);
    if (result) {
            if (Logger::IsDebugEnabled()) {
                Logger::Log("Terminated thread");
            }
        
    }
    return result;
}

bool ThreadManager::SuspendThread(uint32_t thread_id) {
    if (!g_scheduler) return false;
    return g_scheduler->SuspendTask(thread_id);
}

bool ThreadManager::ResumeThread(uint32_t thread_id) {
    if (!g_scheduler) return false;
    return g_scheduler->ResumeTask(thread_id);
}

uint32_t ThreadManager::ExecuteCommand(const char* command) {
    if (!command || !g_scheduler) return 0;
    
    // Create command context
    CommandContext* ctx = (CommandContext*)Heap::Alloc(sizeof(CommandContext));
    if (!ctx) return 0;
    
    // Copy command
    int cmd_len = strlen(command);
    if (cmd_len >= sizeof(ctx->command)) cmd_len = sizeof(ctx->command) - 1;
    for (int i = 0; i < cmd_len; i++) {
        ctx->command[i] = command[i];
    }
    ctx->command[cmd_len] = '\0';
    
    // Get current thread ID
    Thread* current = g_scheduler->GetCurrentTask();
    ctx->caller_thread_id = current ? current->task_id : 0;
    
    // Create command executor thread
    uint32_t thread_id = CreateUserThread((void*)command_executor_thread, 4096,
                                         PRIORITY_NORMAL, "command-executor", ctx->caller_thread_id);
    
    // Pass context to thread (simplified - in real implementation would use proper IPC)
    if (thread_id) {
            if (Logger::IsDebugEnabled()) {
                Logger::Log("Created command executor thread");
            }
       
    } else {
        Heap::Free(ctx);
    }
    
    return thread_id;
}

ThreadEntry* ThreadManager::FindThreadEntry(uint32_t thread_id) {
    ThreadEntry* entry = thread_registry;
    while (entry) {
        if (entry->thread && entry->thread->task_id == thread_id) {
            return entry;
        }
        entry = entry->next;
    }
    return nullptr;
}

void ThreadManager::AddThreadEntry(Thread* thread, SystemThreadType type, 
                                  const char* description, uint32_t parent_id, bool is_system) {
    ThreadEntry* entry = (ThreadEntry*)Heap::Alloc(sizeof(ThreadEntry));
    if (!entry) return;
    
    entry->thread = thread;
    entry->type = type;
    entry->description = description;
    entry->parent_id = parent_id;
    entry->is_system = is_system;
    entry->pid = 0; // default: no PID unless assigned by SpawnProcess
    entry->next = thread_registry;
    
    thread_registry = entry;
    thread_count++;
    
    if (is_system) {
        system_thread_count++;
    } else {
        user_thread_count++;
    }
}

void ThreadManager::RemoveThreadEntry(uint32_t thread_id) {
    ThreadEntry** current = &thread_registry;
    
    while (*current) {
        if ((*current)->thread && (*current)->thread->task_id == thread_id) {
            ThreadEntry* to_remove = *current;
            *current = (*current)->next;
            
            if (to_remove->is_system) {
                system_thread_count--;
            } else {
                user_thread_count--;
            }
            thread_count--;
            
            Heap::Free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

ThreadEntry* ThreadManager::GetThreadEntry(uint32_t thread_id) {
    LockGuard lock(*manager_mutex);
    return FindThreadEntry(thread_id);
}

void ThreadManager::PrintAllThreads() const {
    LockGuard lock(*manager_mutex);
    
    TTY::Write("=== Thread Manager Status ===\n");
    TTY::Write("Total threads: ");
    TTY::WriteHex(thread_count);
    TTY::Write(" (System: ");
    TTY::WriteHex(system_thread_count);
    TTY::Write(", User: ");
    TTY::WriteHex(user_thread_count);
    TTY::Write(")\n");
    
    ThreadEntry* entry = thread_registry;
    while (entry) {
        if (entry->thread) {
            TTY::Write("Thread ");
            TTY::WriteHex(entry->thread->task_id);
            TTY::Write(": ");
            TTY::Write(entry->description);
            TTY::Write(" [");
            TTY::Write(entry->is_system ? "SYSTEM" : "USER");
            TTY::Write("] State: ");
            TTY::Write(entry->thread->GetStateString());
            TTY::Write(" Priority: ");
            TTY::Write(entry->thread->GetPriorityString());
            if (entry->parent_id) {
                TTY::Write(" Parent: ");
                TTY::WriteHex(entry->parent_id);
            }
            TTY::Write("\n");
        }
        entry = entry->next;
    }
}

void ThreadManager::TerminateAllUserThreads() {
    LockGuard lock(*manager_mutex);
    
    ThreadEntry* entry = thread_registry;
    while (entry) {
        if (entry->thread && !entry->is_system) {
            g_scheduler->KillTask(entry->thread->task_id);
        }
        entry = entry->next;
    }
    
    Logger::Log("Terminated all user threads");
}

void ThreadManager::TerminateAllThreads() {
    LockGuard lock(*manager_mutex);
    
    while (thread_registry) {
        ThreadEntry* entry = thread_registry;
        thread_registry = thread_registry->next;
        
        if (entry->thread) {
            g_scheduler->KillTask(entry->thread->task_id);
        }
        Heap::Free(entry);
    }
    
    thread_count = 0;
    system_thread_count = 0;
    user_thread_count = 0;
    
    Logger::Log("Terminated all threads");
}

// --- User process spawning (ELF) ---

// Context passed to the process trampoline
struct ProcLaunchCtx {
    char path[160];
    char name[32];
};

// Use fully qualified name for global filesystem pointer

// Simple global slot to pass ProcLaunchCtx into the next spawned process thread
namespace kos { namespace process {
static ProcLaunchCtx* g_pl_ctx_slot = nullptr;
ProcLaunchCtx* set_proc_launch_ctx_slot(ProcLaunchCtx* v) {
    ProcLaunchCtx* prev = g_pl_ctx_slot;
    g_pl_ctx_slot = v;
    return prev;
}
}} // namespace kos::process

static void elf_process_trampoline() {
    // Retrieve current thread id to resolve registry entry and free context after
    Thread* cur = g_scheduler ? g_scheduler->GetCurrentTask() : nullptr;
    // We embedded the context pointer at the top of the stack space just before the stack base
    // Simpler: store context in thread description via registry lookup; here, we find it by thread id
    // For simplicity, we pass the context pointer via a static TLS-like slot
    ProcLaunchCtx* ctx = g_pl_ctx_slot;
    g_pl_ctx_slot = nullptr;
    if (!ctx) {
        Logger::Log("Spawn: missing context");
        SchedulerAPI::ExitThread();
        return;
    }

    const int8_t* argv0v[1];
    const int8_t* argv0 = (const int8_t*)(ctx->name[0] ? ctx->name : ctx->path);
    argv0v[0] = argv0;
    kos::sys::SetArgs(1, argv0v, (const int8_t*)ctx->path);

    static uint8_t elfBuf[256*1024];
    int32_t n = kos::fs::g_fs_ptr ? kos::fs::g_fs_ptr->ReadFile((const int8_t*)ctx->path, elfBuf, sizeof(elfBuf)) : -1;
    if (n <= 0) {
        TTY::Write((const int8_t*)"spawn: not found: "); TTY::Write((const int8_t*)ctx->path); TTY::PutChar('\n');
        // free ctx
        Heap::Free(ctx);
        SchedulerAPI::ExitThread();
        return;
    }
    bool ok = kos::lib::ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n);
    // free ctx after return
    Heap::Free(ctx);
    (void)ok;
    SchedulerAPI::ExitThread();
}

// PID allocator (separate from scheduler thread IDs)
static uint32_t s_next_pid = 1;

uint32_t ThreadManager::SpawnProcess(const char* elf_path, const char* name, uint32_t* out_pid,
                                     uint32_t stack_size, ThreadPriority priority, uint32_t parent_id) {
    if (!g_scheduler || !elf_path || !*elf_path) return 0;
    // Prepare launch context
    ProcLaunchCtx* ctx = (ProcLaunchCtx*)Heap::Alloc(sizeof(ProcLaunchCtx));
    if (!ctx) return 0;
    // Copy path and optional name
    int i = 0; while (elf_path[i] && i < (int)sizeof(ctx->path)-1) { ctx->path[i] = elf_path[i]; ++i; } ctx->path[i] = 0;
    ctx->name[0] = 0;
    if (name && *name) {
        int j = 0; while (name[j] && j < (int)sizeof(ctx->name)-1) { ctx->name[j] = name[j]; ++j; } ctx->name[j] = 0;
    }

    // Use a slot to pass ctx to the new thread just before creation
    extern ProcLaunchCtx* set_proc_launch_ctx_slot(ProcLaunchCtx*);
    auto prev = set_proc_launch_ctx_slot(ctx);
    (void)prev;
    Thread* thread = g_scheduler->CreateTask((void*)elf_process_trampoline, stack_size, priority,
                                             (name && *name) ? name : "proc");
    if (!thread) {
        // clear slot and free ctx
        (void)set_proc_launch_ctx_slot(nullptr);
        Heap::Free(ctx);
        return 0;
    }

    LockGuard lock(*manager_mutex);
    // Assign PID (first spawn -> PID 1)
    uint32_t pid = s_next_pid++;
    AddThreadEntry(thread, THREAD_USER_PROCESS, (name && *name) ? name : "proc", parent_id, false);
    // Set pid on the entry we just added (head of list)
    if (thread_registry && thread_registry->thread == thread) {
        thread_registry->pid = pid;
    } else {
        // Fallback: find and set
        ThreadEntry* e = FindThreadEntry(thread->task_id);
        if (e) e->pid = pid;
    }
    if (out_pid) *out_pid = pid;
    if (Logger::IsDebugEnabled()) {
        Logger::LogKV("Spawned process", (name && *name) ? name : elf_path);
    }
    return thread->task_id;
}

uint32_t ThreadManager::GetPid(uint32_t thread_id) {
    LockGuard lock(*manager_mutex);
    ThreadEntry* e = FindThreadEntry(thread_id);
    return e ? e->pid : 0;
}

// System thread implementations

extern "C" void kernel_main_thread() {
    Logger::Log("Main kernel thread running");
    
    // Main kernel thread handles system initialization completion
    // and serves as the system supervisor
    while (true) {
        // Yield to other threads
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(100); // Sleep 100ms
    }
}

extern "C" void shell_thread() {
    Logger::Log("Shell thread started");
    
    // Initialize and start the threaded shell if not already running
    if (!kos::console::g_threaded_shell) {
        if (kos::console::ThreadedShellAPI::InitializeShell()) {
            kos::console::ThreadedShellAPI::StartShell();
        } else {
            // Fallback to simple shell run loop if threaded shell fails
            Shell shell;
            shell.Run();
        }
    }

    // Shell main loop
    while (true) {
        // Yield to allow other threads to run
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(50); // Small delay to prevent busy waiting
    }
}

extern "C" void keyboard_thread() {
    Logger::Log("Keyboard thread started");
    
    // Keyboard input handler - processes keyboard events in separate thread
    while (true) {
        // In a real implementation, this would handle keyboard interrupts
        // and forward input to the appropriate threads (shell, applications, etc.)
        
        SchedulerAPI::YieldThread();
        SchedulerAPI::SleepThread(10); // 10ms polling
    }
}

extern "C" void idle_thread() {
    // Idle thread - runs when no other threads are ready
    while (true) {
        // CPU can halt here to save power
        SchedulerAPI::YieldThread();
    }
}

extern "C" void command_executor_thread() {
        if (Logger::IsDebugEnabled()) {
        Logger::Log("Command executor thread started");
    }

    // This thread would execute individual shell commands
    // In a full implementation, it would:
    // 1. Parse the command from the context
    // 2. Execute the appropriate function
    // 3. Return results to the caller
    // 4. Clean up and exit
    
    SchedulerAPI::SleepThread(100); // Simulate command execution
       if (Logger::IsDebugEnabled()) {
        Logger::Log("Command executor thread finished");
    }
    SchedulerAPI::ExitThread();
}

// ThreadManagerAPI implementation

namespace kos::process::ThreadManagerAPI {
    
    bool InitializeThreading() {
        if (!g_thread_manager) {
            g_thread_manager = new ThreadManager();
        }
        
        return g_thread_manager->Initialize();
    }
    
    void StartMultitasking() {
        if (g_thread_manager) {
            g_thread_manager->StartMultitasking();
        }
    }
    
    uint32_t CreateThread(void* entry_point, uint32_t stack_size,
                         ThreadPriority priority, const char* description) {
        if (!g_thread_manager) return 0;
        return g_thread_manager->CreateUserThread(entry_point, stack_size, priority, description);
    }
    
    uint32_t CreateSystemThread(void* entry_point, SystemThreadType type,
                               uint32_t stack_size, ThreadPriority priority,
                               const char* description) {
        if (!g_thread_manager) return 0;
        return g_thread_manager->CreateSystemThread(entry_point, stack_size, priority, type, description);
    }
    
    uint32_t SpawnProcess(const char* elf_path, const char* name, uint32_t* out_pid,
                          uint32_t stack_size, ThreadPriority priority, uint32_t parent_id) {
        if (!g_thread_manager) return 0;
        return g_thread_manager->SpawnProcess(elf_path, name, out_pid, stack_size, priority, parent_id);
    }
    
    bool TerminateThread(uint32_t thread_id) {
        if (!g_thread_manager) return false;
        return g_thread_manager->TerminateThread(thread_id);
    }
    
    uint32_t ExecuteCommand(const char* command) {
        if (!g_thread_manager) return 0;
        return g_thread_manager->ExecuteCommand(command);
    }
    
    uint32_t GetCurrentThreadId() {
        if (!g_scheduler) return 0;
        Thread* current = g_scheduler->GetCurrentTask();
        return current ? current->task_id : 0;
    }
    
    void PrintThreadStatus() {
        if (g_thread_manager) {
            g_thread_manager->PrintAllThreads();
        } else {
            TTY::Write("Thread manager not initialized\n");
        }
    }
}