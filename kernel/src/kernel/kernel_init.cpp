#include <kernel/init.hpp>
#include <common/types.hpp>
#include <memory/pmm.hpp>
#include <memory/paging.hpp>
#include <memory/heap.hpp>
#include <process/scheduler.hpp>
#include <process/timer.hpp>
#include <process/pipe.hpp>
#include <process/message_queue.hpp>
#include <process/thread_manager.hpp>
#include <console/logger.hpp>

// Extern hook to expose timer handler to ServiceManager for uptime profiling
namespace kos { 
    namespace services { 
        extern kos::process::SchedulerTimerHandler* g_timer_handler_for_services; 
    } 
}

using namespace kos;
using namespace kos::common;
using namespace kos::memory;
using namespace kos::process;

namespace kos { 
    namespace kernel {

        void InitCore(arch::x86::hardware::interrupts::InterruptManager* interrupts,
              uint32_t memLowerKB, uint32_t memUpperKB,
              const void* kernel_start, const void* kernel_end,
              const void* multiboot_structure)
        {
            Logger::LogStatus("Initializing core subsystems (PMM/Paging/Heap)", true);

            kos::memory::PMM::Init(memLowerKB, memUpperKB, (phys_addr_t)kernel_start, (phys_addr_t)kernel_end, multiboot_structure);
            Logger::LogStatus("PMM initialized", true);

            kos::memory::Paging::Init((phys_addr_t)kernel_start, (phys_addr_t)kernel_end);
            Logger::LogStatus("Paging enabled", true);

            kos::memory::Heap::Init((virt_addr_t)0x02000000, 2);
            Logger::LogStatus("Kernel heap initialized", true);

            // Initialize scheduler and timer for preemptive multitasking
            kos::process::g_scheduler = new kos::process::Scheduler();
            kos::process::SchedulerTimerHandler* timer_handler = new kos::process::SchedulerTimerHandler(interrupts, kos::process::g_scheduler);
            // Expose timer handler to ServiceManager for uptime profiling.
            kos::services::g_timer_handler_for_services = timer_handler;
            Logger::LogStatus("Scheduler initialized", true);

            // Initialize pipe manager for inter-task communication
            kos::process::g_pipe_manager = new kos::process::PipeManager();
            Logger::LogStatus("Pipe manager initialized", true);

            // Initialize message queue manager for structured IPC
            kos::process::g_message_queue_manager = new kos::process::MessageQueueManager();
            Logger::LogStatus("Message queue manager initialized", true);

            // Temporary VBox-safe path: defer thread manager bootstrap.
            // Early threading init is currently a boot blocker on some VBox runs
            // (serial log stops around "Thr..."). Keep the kernel progressing so
            // graphics/services can initialize and we can diagnose render path issues.
            Logger::LogStatus("Thread manager init deferred", true);
        }
    } 
}
