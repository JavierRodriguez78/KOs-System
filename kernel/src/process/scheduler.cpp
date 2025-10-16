#include <process/scheduler.hpp>
#include <process/thread.h>
#include <memory/heap.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>

using namespace kos::process;
using namespace kos::memory;
using namespace kos::console;

// Global scheduler instance
Scheduler* kos::process::g_scheduler = nullptr;

// TimerHandler implementation
TimerHandler::TimerHandler(Scheduler* sched, uint32_t quantum) 
    : scheduler(sched), quantum_ticks(quantum) {
}

void TimerHandler::HandleTimerInterrupt(uint32_t esp) {
    if (scheduler && scheduler->IsSchedulingEnabled()) {
        scheduler->OnTimerTick(esp);
    }
}

// Scheduler implementation
Scheduler::Scheduler() 
    : current_task(nullptr), sleeping_tasks(nullptr), next_task_id(1), current_tick(0),
      timer_handler(nullptr), scheduling_enabled(false) {
    
    // Initialize priority queues
    for (int i = 0; i < 5; i++) {
        ready_queues[i] = nullptr;
        ready_queue_tails[i] = nullptr;
    }
    
    timer_handler = new TimerHandler(this, 10); // 10 timer ticks per quantum
    Logger::Log("Advanced scheduler initialized");
}

Scheduler::~Scheduler() {
    // Clean up all tasks from priority queues
    for (int priority = 0; priority < 5; priority++) {
        while (ready_queues[priority]) {
            Thread* task = ready_queues[priority];
            ready_queues[priority] = ready_queues[priority]->next;
            ThreadFactory::DestroyThread(task);
        }
    }
    
    // Clean up sleeping tasks
    while (sleeping_tasks) {
        Thread* task = sleeping_tasks;
        sleeping_tasks = sleeping_tasks->next;
        ThreadFactory::DestroyThread(task);
    }
    
    if (current_task) {
        ThreadFactory::DestroyThread(current_task);
    }
    
    if (timer_handler) {
        delete timer_handler;
    }
}

Thread* Scheduler::CreateTask(void* entry_point, uint32_t stack_size, ThreadPriority priority, const char* name) {
    Thread* new_task = ThreadFactory::CreateThread(next_task_id++, entry_point, stack_size, priority, name);
    if (!new_task) {
        Logger::Log("Failed to create task");
        return nullptr;
    }
    
    AddToReadyQueue(new_task);
        if (Logger::IsDebugEnabled()) {
            Logger::Log("Created task");
        }
    return new_task;
}

void Scheduler::AddToReadyQueue(Thread* task) {
    if (!task) return;
    
    task->state = TASK_READY;
    task->next = nullptr;
    
    int priority = (int)task->priority;
    if (priority < 0 || priority >= 5) priority = PRIORITY_NORMAL;
    
    if (!ready_queues[priority]) {
        ready_queues[priority] = ready_queue_tails[priority] = task;
    } else {
        ready_queue_tails[priority]->next = task;
        ready_queue_tails[priority] = task;
    }
}

Thread* Scheduler::RemoveFromReadyQueue() {
    return GetHighestPriorityTask();
}

Thread* Scheduler::GetHighestPriorityTask() {
    // Check priorities from highest (0) to lowest (4)
    for (int priority = 0; priority < 5; priority++) {
        if (ready_queues[priority]) {
            Thread* task = ready_queues[priority];
            ready_queues[priority] = ready_queues[priority]->next;
            
            if (!ready_queues[priority]) {
                ready_queue_tails[priority] = nullptr;
            }
            
            task->next = nullptr;
            return task;
        }
    }
    return nullptr;
}

void Scheduler::ProcessSleepingTasks() {
    Thread** current = &sleeping_tasks;
    while (*current) {
        Thread* task = *current;
        if (current_tick >= task->sleep_until) {
            // Thread should wake up
            *current = task->next;
            AddToReadyQueue(task);
        } else {
            current = &task->next;
        }
    }
}

void Scheduler::Schedule() {
    if (!scheduling_enabled) return;
    
    // Process any sleeping tasks that should wake up
    ProcessSleepingTasks();
    
    // Save current task if it exists and is still running
    if (current_task && current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        AddToReadyQueue(current_task);
    }
    
    // Get next task from priority queues
    Thread* next_task = GetHighestPriorityTask();
    if (!next_task) {
        // No tasks ready, stay with current task or idle
        if (current_task && current_task->state == TASK_READY) {
            current_task->state = TASK_RUNNING;
        } else {
            current_task = nullptr; // Enter idle state
        }
        return;
    }
    
    Thread* old_task = current_task;
    current_task = next_task;
    current_task->state = TASK_RUNNING;
    
    // Set time slice based on priority (higher priority gets more time)
    switch (current_task->priority) {
        case PRIORITY_CRITICAL: current_task->time_slice = 20; break;
        case PRIORITY_HIGH:     current_task->time_slice = 15; break;
        case PRIORITY_NORMAL:   current_task->time_slice = 10; break;
        case PRIORITY_LOW:      current_task->time_slice = 5;  break;
        case PRIORITY_IDLE:     current_task->time_slice = 3;  break;
    }
    
    // Context switch if we're switching tasks
    if (old_task && old_task != current_task) {
        // For now, simple context switching - just update stack pointers
        // SwitchContext(&old_task->context, &current_task->context);
    }
}

void Scheduler::Yield() {
    if (!scheduling_enabled || !current_task) return;
    
    current_task->time_slice = 0; // Force reschedule
    Schedule();
}

void Scheduler::TerminateCurrentTask() {
    if (!current_task) return;
    
    current_task->state = TASK_TERMINATED;
    
    Logger::Log("Terminated task");
    
    // Get next task
    Thread* terminated_task = current_task;
    current_task = nullptr;
    Schedule();
    
    // Clean up terminated task
    ThreadFactory::DestroyThread(terminated_task);
}

void Scheduler::BlockCurrentTask() {
    if (!current_task) return;
    
    current_task->state = TASK_BLOCKED;
    Schedule();
}

void Scheduler::UnblockTask(uint32_t task_id) {
    // Use FindTask to locate the task
    Thread* task = FindTask(task_id);
    if (task && task->state == TASK_BLOCKED) {
        task->state = TASK_READY;
        AddToReadyQueue(task);
    }
}

uint32_t Scheduler::OnTimerTick(uint32_t esp) {
    if (!scheduling_enabled) return esp;
    
    current_tick++; // Increment global tick counter
    
    if (!current_task) return esp;
    
    // Update runtime statistics
    current_task->total_runtime++;
    
    // Decrement time slice
    if (current_task->time_slice > 0) {
        current_task->time_slice--;
    }
    
    // If time slice expired and there are other tasks ready, preempt
    if (current_task->time_slice == 0 && GetHighestPriorityTask()) {
        // Save current task's context from interrupt stack frame
        SaveContextFromInterrupt(&current_task->context, esp);
        
        // Move current task to ready queue
        current_task->state = TASK_READY;
        AddToReadyQueue(current_task);
        
        // Get next task
        Thread* next_task = GetHighestPriorityTask();
        if (next_task) {
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            
            // Set time slice based on priority
            switch (current_task->priority) {
                case PRIORITY_CRITICAL: current_task->time_slice = 20; break;
                case PRIORITY_HIGH:     current_task->time_slice = 15; break;
                case PRIORITY_NORMAL:   current_task->time_slice = 10; break;
                case PRIORITY_LOW:      current_task->time_slice = 5;  break;
                case PRIORITY_IDLE:     current_task->time_slice = 3;  break;
            }
            
            // Return new task's stack pointer for interrupt return
            return RestoreContextToInterrupt(&current_task->context);
        }
    }
    
    return esp;
}

void Scheduler::EnablePreemption() {
    scheduling_enabled = true;
    Logger::Log("Preemptive scheduling enabled");
}

void Scheduler::DisablePreemption() {
    scheduling_enabled = false;
    Logger::Log("Preemptive scheduling disabled");
}

uint32_t Scheduler::GetTaskCount() const {
    uint32_t count = 0;
    
    // Count tasks in priority queues
    for (int priority = 0; priority < 5; priority++) {
        Thread* task = ready_queues[priority];
        while (task) {
            count++;
            task = task->next;
        }
    }
    
    // Count sleeping tasks
    Thread* task = sleeping_tasks;
    while (task) {
        count++;
        task = task->next;
    }
    
    // Count current task
    if (current_task) count++;
    
    return count;
}

// Advanced thread control functions
Thread* Scheduler::FindTask(uint32_t task_id) {
    // Check current task
    if (current_task && current_task->task_id == task_id) {
        return current_task;
    }
    
    // Check priority queues
    for (int priority = 0; priority < 5; priority++) {
        Thread* task = ready_queues[priority];
        while (task) {
            if (task->task_id == task_id) return task;
            task = task->next;
        }
    }
    
    // Check sleeping tasks
    Thread* task = sleeping_tasks;
    while (task) {
        if (task->task_id == task_id) return task;
        task = task->next;
    }
    
    return nullptr;
}

bool Scheduler::SuspendTask(uint32_t task_id) {
    Thread* task = FindTask(task_id);
    if (!task || task->state == TASK_TERMINATED) return false;
    
    if (task == current_task) {
        current_task->state = TASK_SUSPENDED;
        Schedule(); // Switch to another task
    } else {
        // Remove from ready queue if it's there
        task->state = TASK_SUSPENDED;
        // Note: We'd need to properly remove it from the priority queue here
    }
    
    return true;
}

bool Scheduler::ResumeTask(uint32_t task_id) {
    Thread* task = FindTask(task_id);
    if (!task || task->state != TASK_SUSPENDED) return false;
    
    task->state = TASK_READY;
    AddToReadyQueue(task);
    return true;
}

bool Scheduler::KillTask(uint32_t task_id) {
    Thread* task = FindTask(task_id);
    if (!task || task->state == TASK_TERMINATED) return false;
    
    task->state = TASK_TERMINATED;
    
    if (task == current_task) {
        TerminateCurrentTask();
    } else {
        // Remove from queues and free memory
        // Note: Would need proper queue removal here
        if (task->stack_base) {
            Heap::Free(task->stack_base);
            task->stack_base = nullptr;
        }
        delete task;
    }
    
    return true;
}

bool Scheduler::SleepTask(uint32_t task_id, uint32_t milliseconds) {
    Thread* task = FindTask(task_id);
    if (!task || task->state == TASK_TERMINATED) return false;
    
    // Convert milliseconds to timer ticks (assuming 100 Hz timer)
    uint32_t ticks = (milliseconds * 100) / 1000;
    if (ticks == 0) ticks = 1;
    
    task->state = TASK_SLEEPING;
    task->sleep_until = current_tick + ticks;
    
    // Add to sleeping tasks list
    task->next = sleeping_tasks;
    sleeping_tasks = task;
    
    if (task == current_task) {
        current_task = nullptr;
        Schedule(); // Switch to another task
    }
    
    return true;
}

bool Scheduler::SetTaskPriority(uint32_t task_id, ThreadPriority new_priority) {
    Thread* task = FindTask(task_id);
    if (!task || task->state == TASK_TERMINATED) return false;
    
    task->priority = new_priority;
    
    // If task is in ready queue, we should move it to the correct priority queue
    // For simplicity, just update the priority - it will be in the correct queue next time it's scheduled
    
    return true;
}

uint32_t Scheduler::GetTaskCountByState(TaskState state) const {
    uint32_t count = 0;
    
    // Check current task
    if (current_task && current_task->state == state) count++;
    
    // Check priority queues (all ready tasks)
    if (state == TASK_READY) {
        for (int priority = 0; priority < 5; priority++) {
            Thread* task = ready_queues[priority];
            while (task) {
                count++;
                task = task->next;
            }
        }
    }
    
    // Check sleeping tasks
    if (state == TASK_SLEEPING) {
        Thread* task = sleeping_tasks;
        while (task) {
            count++;
            task = task->next;
        }
    }
    
    return count;
}

void Scheduler::PrintTaskList() const {
    TTY::Write("=== Task List ===\n");
    
    if (current_task) {
        TTY::Write("RUNNING: ID=");
        TTY::WriteHex(current_task->task_id);
        TTY::Write(" Priority=");
        TTY::WriteHex(current_task->priority);
        TTY::Write(" Runtime=");
        TTY::WriteHex(current_task->total_runtime);
        TTY::Write("\n");
    }
    
    // Show ready tasks by priority
    for (int priority = 0; priority < 5; priority++) {
        Thread* task = ready_queues[priority];
        while (task) {
            TTY::Write("READY: ID=");
            TTY::WriteHex(task->task_id);
            TTY::Write(" Priority=");
            TTY::WriteHex(task->priority);
            TTY::Write(" Runtime=");
            TTY::WriteHex(task->total_runtime);
            TTY::Write("\n");
            task = task->next;
        }
    }
    
    // Show sleeping tasks
    Thread* task = sleeping_tasks;
    while (task) {
        TTY::Write("SLEEPING: ID=");
        TTY::WriteHex(task->task_id);
        TTY::Write(" WakeAt=");
        TTY::WriteHex(task->sleep_until);
        TTY::Write("\n");
        task = task->next;
    }
}

// Helper functions for interrupt-based context switching
void Scheduler::SaveContextFromInterrupt(CPUContext* context, uint32_t esp) {
    // The interrupt stack frame looks like this (from top to bottom):
    // [gs][fs][es][ds] (pushed by our interrupt stub)
    // [edi][esi][ebp][esp_orig][ebx][edx][ecx][eax] (pusha)
    // [eip][cs][eflags] (pushed by CPU during interrupt)
    // OR [eip][cs][eflags][esp][ss] (if privilege level change)
    
    uint32_t* stack = (uint32_t*)esp;
    
    // Restore segment registers (top of our stack frame)
    context->gs = stack[0];
    context->fs = stack[1]; 
    context->es = stack[2];
    context->ds = stack[3];
    
    // Restore general purpose registers (pusha order)
    context->edi = stack[4];
    context->esi = stack[5];
    context->ebp = stack[6];
    context->esp_dump = stack[7]; // Original ESP
    context->ebx = stack[8];
    context->edx = stack[9];
    context->ecx = stack[10];
    context->eax = stack[11];
    
    // Restore CPU-saved registers
    context->eip = stack[12];
    context->cs = stack[13];
    context->eflags = stack[14];
    
    // For kernel tasks, we don't have ESP/SS on stack
    // Set them to current kernel values
    context->esp = esp + 15 * sizeof(uint32_t); // Point after interrupt frame
    context->ss = 0x10; // Kernel data segment
}

uint32_t Scheduler::RestoreContextToInterrupt(CPUContext* context) {
    // Allocate space for interrupt stack frame
    uint32_t new_esp = context->esp - 15 * sizeof(uint32_t);
    uint32_t* stack = (uint32_t*)new_esp;
    
    // Set up segment registers (will be popped by interrupt return stub)
    stack[0] = context->gs;
    stack[1] = context->fs;
    stack[2] = context->es;
    stack[3] = context->ds;
    
    // Set up general purpose registers (will be popped by popa)
    stack[4] = context->edi;
    stack[5] = context->esi;
    stack[6] = context->ebp;
    stack[7] = context->esp_dump;
    stack[8] = context->ebx;
    stack[9] = context->edx;
    stack[10] = context->ecx;
    stack[11] = context->eax;
    
    // Set up CPU registers (will be popped by iret)
    stack[12] = context->eip;
    stack[13] = context->cs;
    stack[14] = context->eflags;
    
    return new_esp;
}

// Scheduler API implementation (process namespace)
namespace kos::process::SchedulerAPI {
    uint32_t CreateThread(void* entry_point, uint32_t stack_size, ThreadPriority priority, const char* name) {
        if (!g_scheduler) return 0;
        
        Thread* task = g_scheduler->CreateTask(entry_point, stack_size, priority, name);
        return task ? task->task_id : 0;
    }
    
    void YieldThread() {
        if (g_scheduler) {
            g_scheduler->Yield();
        }
    }
    
    void ExitThread() {
        if (g_scheduler) {
            g_scheduler->TerminateCurrentTask();
        }
    }
    
    void SleepThread(uint32_t milliseconds) {
        if (g_scheduler && g_scheduler->GetCurrentTask()) {
            g_scheduler->SleepTask(g_scheduler->GetCurrentTask()->task_id, milliseconds);
        }
    }
    
    bool SuspendThread(uint32_t thread_id) {
        if (!g_scheduler) return false;
        return g_scheduler->SuspendTask(thread_id);
    }
    
    bool ResumeThread(uint32_t thread_id) {
        if (!g_scheduler) return false;
        return g_scheduler->ResumeTask(thread_id);
    }
    
    bool KillThread(uint32_t thread_id) {
        if (!g_scheduler) return false;
        return g_scheduler->KillTask(thread_id);
    }
    
    bool SetThreadPriority(uint32_t thread_id, ThreadPriority priority) {
        if (!g_scheduler) return false;
        return g_scheduler->SetTaskPriority(thread_id, priority);
    }
    
    uint32_t GetCurrentThreadId() {
        if (g_scheduler && g_scheduler->GetCurrentTask()) {
            return g_scheduler->GetCurrentTask()->task_id;
        }
        return 0;
    }
    
    uint32_t GetThreadCount() {
        if (g_scheduler) {
            return g_scheduler->GetTaskCount();
        }
        return 0;
    }
}