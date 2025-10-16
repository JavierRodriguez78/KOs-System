#include <process/thread.h>
#include <memory/heap.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>

using namespace kos::process;
using namespace kos::memory;
using namespace kos::lib;
using namespace kos::console;

// Thread implementation

Thread::Thread() 
    : task_id(0), state(TASK_READY), priority(PRIORITY_NORMAL), 
      stack_base(nullptr), stack_size(0), time_slice(0), sleep_until(0), 
      total_runtime(0), name("unnamed"), next(nullptr) {
    memset(&context, 0, sizeof(CPUContext));
}

Thread::Thread(uint32_t id, void* entry_point, uint32_t stack_sz, 
               ThreadPriority prio, const char* thread_name) 
    : task_id(id), state(TASK_READY), priority(prio), stack_base(nullptr),
      stack_size(stack_sz), time_slice(0), sleep_until(0), total_runtime(0), 
      name(thread_name), next(nullptr) {
    
    memset(&context, 0, sizeof(CPUContext));
    Initialize(id, entry_point, stack_sz, prio, thread_name);
}

Thread::~Thread() {
    Cleanup();
}

bool Thread::Initialize(uint32_t id, void* entry_point, uint32_t stack_sz, 
                       ThreadPriority prio, const char* thread_name) {
    task_id = id;
    priority = prio;
    stack_size = stack_sz;
    name = thread_name;
    state = TASK_READY;
    time_slice = ThreadFactory::CalculateTimeSlice(priority);
    
    // Allocate and setup stack
    if (!AllocateStack(stack_size)) {
        Logger::Log("Failed to allocate stack for thread");
        return false;
    }
    
    SetupInitialStack(entry_point);
    return true;
}

void Thread::Cleanup() {
    FreeStack();
    state = TASK_TERMINATED;
}

bool Thread::AllocateStack(uint32_t size) {
    if (stack_base) {
        FreeStack(); // Free existing stack
    }
    
    stack_base = (uint32_t*)Heap::Alloc(size);
    if (!stack_base) {
        stack_size = 0;
        return false;
    }
    
    stack_size = size;
    
    // Initialize stack memory
    memset(stack_base, 0, stack_size);
    
    return true;
}

void Thread::FreeStack() {
    if (stack_base) {
        Heap::Free(stack_base);
        stack_base = nullptr;
        stack_size = 0;
    }
}

void Thread::SetupInitialStack(void* entry_point) {
    if (!stack_base) return;
    
    // Initialize CPU context
    memset(&context, 0, sizeof(CPUContext));
    
    // Set up initial stack pointer (stack grows downward)
    context.esp = (uint32_t)stack_base + stack_size - sizeof(uint32_t);
    context.ss = 0x10;  // Kernel data segment
    context.cs = 0x08;  // Kernel code segment
    context.ds = 0x10;  // Data segment
    context.es = 0x10;
    context.fs = 0x10;
    context.gs = 0x10;
    
    // Set entry point
    context.eip = (uint32_t)entry_point;
    
    // Set initial EFLAGS (enable interrupts)
    context.eflags = 0x202;  // IF flag set
}

const char* Thread::GetStateString() const {
    return ThreadUtils::StateToString(state);
}

const char* Thread::GetPriorityString() const {
    return ThreadUtils::PriorityToString(priority);
}

void Thread::PrintInfo() const {
    TTY::Write("Thread ID: ");
    TTY::WriteHex(task_id);
    TTY::Write(" Name: ");
    TTY::Write(name);
    TTY::Write(" State: ");
    TTY::Write(GetStateString());
    TTY::Write(" Priority: ");
    TTY::Write(GetPriorityString());
    TTY::Write(" Runtime: ");
    TTY::WriteHex(total_runtime);
    TTY::Write(" ticks\n");
}

// ThreadFactory implementation

Thread* ThreadFactory::CreateThread(uint32_t id, void* entry_point, 
                                   uint32_t stack_size, ThreadPriority priority,
                                   const char* name) {
    Thread* thread = new Thread();
    if (!thread) {
        return nullptr;
    }
    
    if (!thread->Initialize(id, entry_point, stack_size, priority, name)) {
        delete thread;
        return nullptr;
    }
    
    return thread;
}

void ThreadFactory::DestroyThread(Thread* thread) {
    if (thread) {
        thread->Cleanup();
        delete thread;
    }
}

bool ThreadFactory::SetupThreadStack(Thread* thread, void* entry_point) {
    if (!thread) return false;
    
    // This is handled by Thread::SetupInitialStack
    return true;
}

uint32_t ThreadFactory::CalculateTimeSlice(ThreadPriority priority) {
    switch (priority) {
        case PRIORITY_CRITICAL: return 20;
        case PRIORITY_HIGH:     return 15;
        case PRIORITY_NORMAL:   return 10;
        case PRIORITY_LOW:      return 5;
        case PRIORITY_IDLE:     return 3;
        default:               return 10;
    }
}

// ThreadUtils implementation

namespace kos::process::ThreadUtils {
    
    const char* StateToString(TaskState state) {
        switch (state) {
            case TASK_READY:      return "READY";
            case TASK_RUNNING:    return "RUNNING";
            case TASK_BLOCKED:    return "BLOCKED";
            case TASK_SLEEPING:   return "SLEEPING";
            case TASK_SUSPENDED:  return "SUSPENDED";
            case TASK_TERMINATED: return "TERMINATED";
            default:             return "UNKNOWN";
        }
    }
    
    const char* PriorityToString(ThreadPriority priority) {
        switch (priority) {
            case PRIORITY_CRITICAL: return "CRITICAL";
            case PRIORITY_HIGH:     return "HIGH";
            case PRIORITY_NORMAL:   return "NORMAL";
            case PRIORITY_LOW:      return "LOW";
            case PRIORITY_IDLE:     return "IDLE";
            default:               return "UNKNOWN";
        }
    }
    
    bool IsValidPriority(ThreadPriority priority) {
        return priority >= PRIORITY_CRITICAL && priority <= PRIORITY_IDLE;
    }
    
    bool IsValidState(TaskState state) {
        return state >= TASK_READY && state <= TASK_TERMINATED;
    }
    
    uint32_t MillisecondsToTicks(uint32_t milliseconds, uint32_t timer_frequency) {
        if (timer_frequency == 0) timer_frequency = 100; // Default 100 Hz
        uint32_t ticks = (milliseconds * timer_frequency) / 1000;
        return ticks > 0 ? ticks : 1; // Minimum 1 tick
    }
    
    uint32_t TicksToMilliseconds(uint32_t ticks, uint32_t timer_frequency) {
        if (timer_frequency == 0) timer_frequency = 100; // Default 100 Hz
        return (ticks * 1000) / timer_frequency;
    }
}