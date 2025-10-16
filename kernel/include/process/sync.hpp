#ifndef __KOS__PROCESS__SYNC_H
#define __KOS__PROCESS__SYNC_H

#include <common/types.hpp>
#include <process/thread.h>
#include <process/scheduler.hpp>

using namespace kos::common;

namespace kos {
    namespace process {

        // Forward declaration
        class Scheduler;
        extern Scheduler* g_scheduler;

        // Mutex implementation
        class Mutex {
        private:
            uint32_t owner_thread_id;  // Thread that owns the mutex (0 = unlocked)
            Thread* waiting_queue;       // Queue of threads waiting for this mutex
            bool locked;

        public:
            Mutex();
            ~Mutex();

            // Lock the mutex (blocks if already locked)
            void Lock();
            
            // Try to lock the mutex (returns false if already locked)
            bool TryLock();
            
            // Unlock the mutex
            void Unlock();
            
            // Check if the mutex is locked
            bool IsLocked() const { return locked; }
        };

        // Semaphore implementation
        class Semaphore {
        private:
            uint32_t count;            // Current semaphore count
            uint32_t max_count;        // Maximum count
            Thread* waiting_queue;       // Queue of threads waiting for this semaphore

        public:
            Semaphore(uint32_t initial_count = 1, uint32_t maximum = 1);
            ~Semaphore();

            // Wait for semaphore (decrements count, blocks if 0)
            void Wait();
            
            // Try to wait for semaphore (returns false if count is 0)
            bool TryWait();
            
            // Signal semaphore (increments count, wakes waiting thread if any)
            void Signal();
            
            // Get current count
            uint32_t GetCount() const { return count; }
        };

        // Condition Variable implementation
        class ConditionVariable {
        private:
            Thread* waiting_queue;       // Queue of threads waiting on this condition

        public:
            ConditionVariable();
            ~ConditionVariable();

            // Wait for condition (must be called with mutex locked)
            void Wait(Mutex& mutex);
            
            // Signal one waiting thread
            void Signal();
            
            // Signal all waiting threads
            void Broadcast();
        };

        // RAII lock guard for automatic unlocking
        class LockGuard {
        private:
            Mutex& mutex;

        public:
            explicit LockGuard(Mutex& m) : mutex(m) {
                mutex.Lock();
            }
            
            ~LockGuard() {
                mutex.Unlock();
            }

            // Prevent copying
            LockGuard(const LockGuard&) = delete;
            LockGuard& operator=(const LockGuard&) = delete;
        };

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__SYNC_H