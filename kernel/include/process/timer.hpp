#ifndef __KOS__PROCESS__TIMER_H
#define __KOS__PROCESS__TIMER_H

#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <hardware/port.hpp>

using namespace kos::common;
using namespace kos::hardware;

namespace kos {
    namespace process {

        // Forward declaration
        class Scheduler;

        // Programmable Interval Timer (PIT) controller
        class PIT {
        private:
            static const uint16_t PIT_CHANNEL_0 = 0x40;  // Channel 0 data port
            static const uint16_t PIT_CHANNEL_1 = 0x41;  // Channel 1 data port
            static const uint16_t PIT_CHANNEL_2 = 0x42;  // Channel 2 data port
            static const uint16_t PIT_COMMAND = 0x43;    // Command register
            
            static const uint32_t PIT_FREQUENCY = 1193182; // PIT input frequency in Hz
            
        public:
            // Set the frequency of timer interrupts (in Hz)
            static void SetFrequency(uint32_t frequency);
            
            // Get current frequency
            static uint32_t GetFrequency() { return current_frequency; }
            
        private:
            static uint32_t current_frequency;
        };

        // Timer interrupt handler for scheduling
        class SchedulerTimerHandler : public InterruptHandler {
        private:
            Scheduler* scheduler;
            uint32_t tick_count;
            uint32_t frequency;
            
        public:
            SchedulerTimerHandler(InterruptManager* interrupt_manager, Scheduler* sched);
            virtual uint32_t HandleInterrupt(uint32_t esp) override;
            
            uint32_t GetTickCount() const { return tick_count; }
            uint32_t GetFrequency() const { return frequency; }
            void SetScheduler(Scheduler* sched) { scheduler = sched; }
        };

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__TIMER_H