#include <process/sync.hpp>
#include <process/scheduler.hpp>
#include <console/logger.hpp>

using namespace kos::process;
using namespace kos::console;

// Mutex implementation
Mutex::Mutex() : owner_thread_id(0), waiting_queue(nullptr), locked(false) {
}

Mutex::~Mutex() {
    while (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}

void Mutex::Lock() {
    if (!g_scheduler) return;
    
    uint32_t current_thread = 0;
    if (g_scheduler->GetCurrentTask()) {
        current_thread = g_scheduler->GetCurrentTask()->task_id;
    }
    
    while (true) {
        if (!locked) {
            locked = true;
            owner_thread_id = current_thread;
            return;
        }
        
        if (owner_thread_id == current_thread) {
            return;
        }
        
        if (g_scheduler) {
            g_scheduler->Yield();
        }
    }
}

bool Mutex::TryLock() {
    if (!g_scheduler) return false;
    
    uint32_t current_thread = 0;
    if (g_scheduler->GetCurrentTask()) {
        current_thread = g_scheduler->GetCurrentTask()->task_id;
    }
    
    if (!locked) {
        locked = true;
        owner_thread_id = current_thread;
        return true;
    }
    
    if (owner_thread_id == current_thread) {
        return true;
    }
    
    return false;
}

void Mutex::Unlock() {
    if (!g_scheduler) return;
    
    uint32_t current_thread = 0;
    if (g_scheduler->GetCurrentTask()) {
        current_thread = g_scheduler->GetCurrentTask()->task_id;
    }
    
    if (owner_thread_id != current_thread) {
        return;
    }
    
    locked = false;
    owner_thread_id = 0;
    
    if (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}

// Semaphore implementation
Semaphore::Semaphore(uint32_t initial_count, uint32_t maximum) 
    : count(initial_count), max_count(maximum), waiting_queue(nullptr) {
}

Semaphore::~Semaphore() {
    while (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}

void Semaphore::Wait() {
    if (!g_scheduler) return;
    
    while (true) {
        if (count > 0) {
            count--;
            return;
        }
        
        if (g_scheduler) {
            g_scheduler->Yield();
        }
    }
}

bool Semaphore::TryWait() {
    if (!g_scheduler) return false;
    
    if (count > 0) {
        count--;
        return true;
    }
    
    return false;
}

void Semaphore::Signal() {
    if (!g_scheduler) return;
    
    if (count < max_count) {
        count++;
        
        if (waiting_queue) {
            Thread* task = waiting_queue;
            waiting_queue = waiting_queue->next;
            if (g_scheduler) {
                g_scheduler->UnblockTask(task->task_id);
            }
        }
    }
}

// ConditionVariable implementation
ConditionVariable::ConditionVariable() : waiting_queue(nullptr) {
}

ConditionVariable::~ConditionVariable() {
    while (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}

void ConditionVariable::Wait(Mutex& mutex) {
    if (!g_scheduler) return;
    
    mutex.Unlock();
    
    if (g_scheduler) {
        g_scheduler->Yield();
    }
    
    mutex.Lock();
}

void ConditionVariable::Signal() {
    if (!g_scheduler) return;
    
    if (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}

void ConditionVariable::Broadcast() {
    if (!g_scheduler) return;
    
    while (waiting_queue) {
        Thread* task = waiting_queue;
        waiting_queue = waiting_queue->next;
        if (g_scheduler) {
            g_scheduler->UnblockTask(task->task_id);
        }
    }
}
