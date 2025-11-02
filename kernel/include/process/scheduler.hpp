     
#ifndef __KOS__PROCESS__SCHEDULER_H
#define __KOS__PROCESS__SCHEDULER_H

#include <common/types.hpp>
#include <memory/memory.hpp>
#include <process/thread.h>

using namespace kos::common;

namespace kos {
    namespace process {

        // Forward declaration
        class Scheduler;

        // Timer interrupt handler for preemptive scheduling
        class TimerHandler {
        private:
            Scheduler* scheduler;
            uint32_t quantum_ticks;         // Time slice duration in timer ticks
            
        public:
            TimerHandler(Scheduler* sched, uint32_t quantum = 10);
            void HandleTimerInterrupt(uint32_t esp);
            void SetQuantum(uint32_t quantum) { quantum_ticks = quantum; }
        };

        // Round-robin scheduler
        class Scheduler {
        private:
            Thread* current_task;             // Currently running task
            Thread* ready_queues[5];          // Priority-based ready queues (one per priority)
            Thread* ready_queue_tails[5];     // Tail pointers for each priority queue
            Thread* sleeping_tasks;           // List of sleeping tasks
            uint32_t next_task_id;          // For generating unique task IDs
            uint32_t current_tick;          // Current timer tick count
            TimerHandler* timer_handler;    // Timer interrupt handler
            bool scheduling_enabled;        // Whether preemptive scheduling is active

            // Internal helper methods
            void AddToReadyQueue(Thread* task);
            Thread* RemoveFromReadyQueue();
            Thread* GetHighestPriorityTask();
            void ProcessSleepingTasks();

        public:
            Scheduler();
            ~Scheduler();

            // Core scheduling functions
            Thread* CreateTask(void* entry_point, uint32_t stack_size = 4096, 
                           ThreadPriority priority = PRIORITY_NORMAL, const char* name = "unnamed");
            void Schedule();                // Switch to next ready task
            void Yield();                   // Voluntarily give up CPU
            void TerminateCurrentTask();
            void BlockCurrentTask();
            void UnblockTask(uint32_t task_id);

            // Advanced thread control
            bool SuspendTask(uint32_t task_id);
            bool ResumeTask(uint32_t task_id);
            bool KillTask(uint32_t task_id);
            bool SleepTask(uint32_t task_id, uint32_t milliseconds);
            bool SetTaskPriority(uint32_t task_id, ThreadPriority new_priority);
            
            // Thread information
            Thread* FindTask(uint32_t task_id);
            uint32_t GetTaskCount() const;
            uint32_t GetTaskCountByState(TaskState state) const;
            void PrintTaskList() const;

            // Timer integration
            uint32_t OnTimerTick(uint32_t esp); // Called by timer interrupt - returns new ESP
            void EnablePreemption();
            void DisablePreemption();

            // Context switching helpers
            void SaveContextFromInterrupt(CPUContext* context, uint32_t esp);
            uint32_t RestoreContextToInterrupt(CPUContext* context);

            // Getters
            Thread* GetCurrentTask() const { return current_task; }
            bool IsSchedulingEnabled() const { return scheduling_enabled; }

            // Public accessor for ready queues
            Thread* GetReadyQueue(int prio) const { return ready_queues[prio]; }
            Thread* GetReadyQueueTail(int prio) const { return ready_queue_tails[prio]; }    

            // Public accessor for sleeping tasks
            Thread* GetSleepingTasks() const { return sleeping_tasks; }
        };

        // Global scheduler instance
        extern Scheduler* g_scheduler;

        // API functions for applications
        namespace SchedulerAPI {
            uint32_t CreateThread(void* entry_point, uint32_t stack_size = 4096, 
                                ThreadPriority priority = PRIORITY_NORMAL, const char* name = "unnamed");
            void YieldThread();
            void ExitThread();
            void SleepThread(uint32_t milliseconds);
            bool SuspendThread(uint32_t thread_id);
            bool ResumeThread(uint32_t thread_id);
            bool KillThread(uint32_t thread_id);
            bool SetThreadPriority(uint32_t thread_id, ThreadPriority priority);
            uint32_t GetCurrentThreadId();
            uint32_t GetThreadCount();
        }

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__SCHEDULER_H