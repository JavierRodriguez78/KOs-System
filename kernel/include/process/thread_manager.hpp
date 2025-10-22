#ifndef __KOS__PROCESS__THREAD_MANAGER_H
#define __KOS__PROCESS__THREAD_MANAGER_H

#include <common/types.hpp>
#include <process/thread.h>
#include <process/sync.hpp>

using namespace kos::common;

namespace kos {
    namespace process {

        // Forward declarations
        class Scheduler;
        class Pipe;

        // System thread types
        enum SystemThreadType {
            THREAD_KERNEL_MAIN = 0,     // Main kernel thread
            THREAD_SHELL = 1,           // Shell/console thread
            THREAD_KEYBOARD = 2,        // Keyboard input handler
            THREAD_TIMER = 3,           // Timer management
            THREAD_IDLE = 4,            // Idle thread
            THREAD_USER_PROCESS = 5,    // User application threads
            THREAD_SYSTEM_SERVICE = 6,  // System service threads
            THREAD_DEMO = 7             // Demo/test threads
        };

        // Thread registry entry
        struct ThreadEntry {
            Thread* thread;
            SystemThreadType type;
            const char* description;
            uint32_t parent_id;         // Parent thread ID
            bool is_system;             // System vs user thread
            // Lightweight process identifier for user processes (separate from scheduler thread IDs)
            // 0 means no PID assigned (system threads or helper threads)
            uint32_t pid;
            ThreadEntry* next;          // Linked list
        };

        // Comprehensive thread manager
        class ThreadManager {
        private:
            ThreadEntry* thread_registry;  // Registry of all threads
            uint32_t thread_count;          // Total number of threads
            uint32_t system_thread_count;   // Number of system threads
            uint32_t user_thread_count;     // Number of user threads
            
            Mutex* manager_mutex;           // Thread-safe access
            Thread* main_thread;            // Main kernel thread
            Thread* shell_thread;           // Shell thread
            Thread* idle_thread;            // Idle thread
            
            bool threading_initialized;    // Whether threading is active
            
            // Internal helpers
            ThreadEntry* FindThreadEntry(uint32_t thread_id);
            void AddThreadEntry(Thread* thread, SystemThreadType type, 
                              const char* description, uint32_t parent_id, bool is_system);
            void RemoveThreadEntry(uint32_t thread_id);
            
        public:
            ThreadManager();
            ~ThreadManager();
            
            // System initialization
            bool Initialize();
            void StartMultitasking();
            void CreateSystemThreads();
            
            // Thread creation and management
            uint32_t CreateSystemThread(void* entry_point, uint32_t stack_size,
                                      ThreadPriority priority, SystemThreadType type,
                                      const char* description, uint32_t parent_id = 0);
            uint32_t CreateUserThread(void* entry_point, uint32_t stack_size,
                                    ThreadPriority priority, const char* description,
                                    uint32_t parent_id = 0);
            // Spawn a user process from an ELF path in a dedicated user thread and assign a PID.
            // Returns thread_id; optionally writes the assigned PID to out_pid.
            // argv0/name may be null, in which case the basename of path is used.
            uint32_t SpawnProcess(const char* elf_path, const char* name = nullptr, uint32_t* out_pid = nullptr,
                                  uint32_t stack_size = 8192, ThreadPriority priority = PRIORITY_NORMAL,
                                  uint32_t parent_id = 0);
            bool TerminateThread(uint32_t thread_id);
            bool SuspendThread(uint32_t thread_id);
            bool ResumeThread(uint32_t thread_id);
            
            // Process management
            uint32_t ExecuteCommand(const char* command);
            uint32_t LaunchApplication(const char* app_path);
            
            // Thread information
            ThreadEntry* GetThreadEntry(uint32_t thread_id);
            uint32_t GetThreadCount() const { return thread_count; }
            uint32_t GetSystemThreadCount() const { return system_thread_count; }
            uint32_t GetUserThreadCount() const { return user_thread_count; }
            void PrintAllThreads() const;
            void PrintSystemThreads() const;
            void PrintUserThreads() const;
            // Look up PID for a given thread id (0 if none)
            uint32_t GetPid(uint32_t thread_id);
            
            // System threads
            Thread* GetMainThread() const { return main_thread; }
            Thread* GetShellThread() const { return shell_thread; }
            Thread* GetIdleThread() const { return idle_thread; }
            
            // Thread lifecycle callbacks
            void OnThreadCreated(uint32_t thread_id);
            void OnThreadTerminated(uint32_t thread_id);
            void OnThreadSuspended(uint32_t thread_id);
            void OnThreadResumed(uint32_t thread_id);
            
            // System status
            bool IsThreadingActive() const { return threading_initialized; }
            
            // Cleanup
            void TerminateAllUserThreads();
            void TerminateAllThreads();
        };

        // System thread entry points
        extern "C" void kernel_main_thread();
        extern "C" void shell_thread();
        extern "C" void keyboard_thread();
        extern "C" void idle_thread();
        extern "C" void command_executor_thread();

        // Global thread manager instance
        extern ThreadManager* g_thread_manager;

        // Thread Manager API
        namespace ThreadManagerAPI {
            bool InitializeThreading();
            void StartMultitasking();
            
            uint32_t CreateThread(void* entry_point, uint32_t stack_size = 4096,
                                ThreadPriority priority = PRIORITY_NORMAL,
                                const char* description = "user-thread");
            uint32_t CreateSystemThread(void* entry_point, SystemThreadType type,
                                      uint32_t stack_size = 4096,
                                      ThreadPriority priority = PRIORITY_HIGH,
                                      const char* description = "system-thread");
            // Create a user process from an ELF path and assign PID (PID 1 reserved for first spawn).
            uint32_t SpawnProcess(const char* elf_path, const char* name = nullptr, uint32_t* out_pid = nullptr,
                                  uint32_t stack_size = 8192, ThreadPriority priority = PRIORITY_NORMAL,
                                  uint32_t parent_id = 0);
            
            bool TerminateThread(uint32_t thread_id);
            uint32_t ExecuteCommand(const char* command);
            uint32_t GetCurrentThreadId();
            
            void PrintThreadStatus();
        }

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__THREAD_MANAGER_H