#ifndef __KOS__PROCESS__THREAD_H
#define __KOS__PROCESS__THREAD_H

#include <common/types.hpp>
#include <memory/memory.hpp>

using namespace kos::common;

namespace kos {
    namespace process {

        // Thread/Task states
        enum TaskState {
            TASK_READY = 0,      // Ready to run
            TASK_RUNNING = 1,    // Currently executing
            TASK_BLOCKED = 2,    // Waiting for I/O or event
            TASK_SLEEPING = 3,   // Sleeping for a specific time
            TASK_SUSPENDED = 4,  // Manually suspended
            TASK_TERMINATED = 5  // Finished execution
        };

        // Thread priorities (lower number = higher priority)
        enum ThreadPriority {
            PRIORITY_CRITICAL = 0,  // System critical threads
            PRIORITY_HIGH = 1,      // High priority threads
            PRIORITY_NORMAL = 2,    // Normal priority threads  
            PRIORITY_LOW = 3,       // Low priority threads
            PRIORITY_IDLE = 4       // Idle/background threads
        };

        // CPU context structure - all registers that need to be saved/restored
        struct CPUContext {
            // General purpose registers (saved by pusha/popa)
            uint32_t edi, esi, ebp, esp_dump, ebx, edx, ecx, eax;
            
            // Segment registers
            uint32_t ds, es, fs, gs;
            
            // Instruction pointer and flags (saved by interrupt mechanism)
            uint32_t eip;
            uint32_t cs;
            uint32_t eflags;
            uint32_t esp;       // Stack pointer
            uint32_t ss;        // Stack segment
        } __attribute__((packed));

        // Task Control Block (TCB) - represents a thread/task
        class Thread {
        public:
            uint32_t task_id;               // Unique task identifier
            TaskState state;                // Current task state
            ThreadPriority priority;        // Thread priority
            CPUContext context;             // Saved CPU context
            uint32_t* stack_base;           // Base of task's stack
            uint32_t stack_size;            // Size of task's stack in bytes
            uint32_t time_slice;            // Remaining time quantum (in timer ticks)
            uint32_t sleep_until;           // Timer tick when thread should wake up (if sleeping)
            uint32_t total_runtime;         // Total CPU time used (in timer ticks)
            const char* name;               // Thread name for debugging
            Thread* next;                   // Next task in queue (for linked list)
            
            // Constructors
            Thread();
            Thread(uint32_t id, void* entry_point, uint32_t stack_sz = 4096, 
                   ThreadPriority prio = PRIORITY_NORMAL, const char* thread_name = "unnamed");
            
            // Destructor
            ~Thread();

            // Thread management methods
            bool Initialize(uint32_t id, void* entry_point, uint32_t stack_sz, 
                          ThreadPriority prio, const char* thread_name);
            void Cleanup();
            
            // State management
            void SetState(TaskState new_state) { state = new_state; }
            TaskState GetState() const { return state; }
            
            // Priority management
            void SetPriority(ThreadPriority new_priority) { priority = new_priority; }
            ThreadPriority GetPriority() const { return priority; }
            
            // Time management
            void SetTimeSlice(uint32_t ticks) { time_slice = ticks; }
            uint32_t GetTimeSlice() const { return time_slice; }
            void DecrementTimeSlice() { if (time_slice > 0) time_slice--; }
            
            // Runtime statistics
            void IncrementRuntime() { total_runtime++; }
            uint32_t GetTotalRuntime() const { return total_runtime; }
            
            // Sleep management
            void SetSleepUntil(uint32_t tick) { sleep_until = tick; }
            uint32_t GetSleepUntil() const { return sleep_until; }
            bool ShouldWakeUp(uint32_t current_tick) const { 
                return state == TASK_SLEEPING && current_tick >= sleep_until; 
            }
            
            // Stack management
            bool AllocateStack(uint32_t size);
            void FreeStack();
            uint32_t* GetStackBase() const { return stack_base; }
            uint32_t GetStackSize() const { return stack_size; }
            
            // Context management
            void SaveContext(const CPUContext& ctx) { context = ctx; }
            const CPUContext& GetContext() const { return context; }
            CPUContext& GetContext() { return context; }
            
            // Debug info
            const char* GetName() const { return name; }
            void SetName(const char* new_name) { name = new_name; }
            uint32_t GetId() const { return task_id; }
            
            // Utility methods
            const char* GetStateString() const;
            const char* GetPriorityString() const;
            void PrintInfo() const;

        private:
            void SetupInitialStack(void* entry_point);
        };

        // Thread factory for creating threads
        class ThreadFactory {
        public:
            static Thread* CreateThread(uint32_t id, void* entry_point, 
                                      uint32_t stack_size = 4096,
                                      ThreadPriority priority = PRIORITY_NORMAL,
                                      const char* name = "unnamed");
            
            static void DestroyThread(Thread* thread);
            
            // Helper methods for stack setup
            static bool SetupThreadStack(Thread* thread, void* entry_point);
            static uint32_t CalculateTimeSlice(ThreadPriority priority);
        };

        // Thread utilities
        namespace ThreadUtils {
            const char* StateToString(TaskState state);
            const char* PriorityToString(ThreadPriority priority);
            bool IsValidPriority(ThreadPriority priority);
            bool IsValidState(TaskState state);
            uint32_t MillisecondsToTicks(uint32_t milliseconds, uint32_t timer_frequency = 100);
            uint32_t TicksToMilliseconds(uint32_t ticks, uint32_t timer_frequency = 100);
        }

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__THREAD_H