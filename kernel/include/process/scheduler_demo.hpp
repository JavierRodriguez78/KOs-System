#ifndef __KOS__PROCESS__SCHEDULER_DEMO_H
#define __KOS__PROCESS__SCHEDULER_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

// Demo functions to show round-robin scheduling in action
void StartSchedulerDemo();
void ShowSchedulerStats();

#ifdef __cplusplus
}
#endif

namespace kos {
    namespace process {
        
        // Individual demo task functions
        void task_a();
        void task_b();
        void task_c();
        void critical_task();
        void background_task();

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__SCHEDULER_DEMO_H