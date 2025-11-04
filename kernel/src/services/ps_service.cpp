// Process Info Service: outputs all process info in text format for user-space tools like 'top'.


#include <process/scheduler.hpp>
#include <lib/string.hpp>
#include <lib/stdio.hpp>


using namespace kos::process;
using namespace kos::lib;
using namespace kos::sys;

extern Scheduler* g_scheduler;

extern "C" int ps_service_getinfo(char* buffer, int maxlen) {
    if (!buffer || maxlen <= 0) return 0;
    int written = 0;
    auto write_line = [&](uint32_t pid, const char* state, uint32_t prio, uint32_t time, const char* name) {
        const char* nm = name ? name : "unnamed";
        int n = snprintf(buffer + written, (maxlen - written > 0) ? maxlen - written : 0,
                         "%u %s %u %u %s\n", pid, state, prio, time, nm);
        if (n > 0) written += n;
    };
    if (!kos::process::g_scheduler) {
        return snprintf(buffer, maxlen, "No scheduler available\n");
    }
    // Note: header is printed by the 'top' app. Here we output only data rows.
    Thread* current = kos::process::g_scheduler->GetCurrentTask();
    if (current) {
        write_line(current->GetId(), "RUNNING", current->GetPriority(), current->GetTotalRuntime(), current->GetName());
    }
    for (int prio = 0; prio < 5; prio++) {
        Thread* t = kos::process::g_scheduler->GetReadyQueue(prio);
        while (t) {
            write_line(t->GetId(), "READY", t->GetPriority(), t->GetTotalRuntime(), t->GetName());
            t = t->next;
        }
    }
    Thread* s = kos::process::g_scheduler->GetSleepingTasks();
    while (s) {
        write_line(s->GetId(), "SLEEPING", s->GetPriority(), s->GetTotalRuntime(), s->GetName());
        s = s->next;
    }
    return written;
}
