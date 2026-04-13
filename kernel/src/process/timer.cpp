#include <process/timer.hpp>
#include <process/scheduler.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/ps2/ps2.hpp>

using namespace kos::process;
using namespace kos::console;

// PIT implementation
uint32_t PIT::current_frequency = 18; // Default PIT frequency ~18.2 Hz

void PIT::SetFrequency(uint32_t frequency) {
    static TTY tty;
    tty.Write("[PIT] Setting frequency to ");
    tty.WriteHex(frequency);
    tty.Write(" Hz\n");
    
    if (frequency == 0) frequency = 18; // Prevent division by zero
    
    current_frequency = frequency;
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    // Ensure divisor is within valid range
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;
    
    tty.Write("[PIT] Calculated divisor: ");
    tty.WriteHex(divisor);
    tty.Write("\n");
    
    Port8Bit command_port(PIT_COMMAND);
    Port8Bit data_port(PIT_CHANNEL_0);
    
    // Set PIT to mode 3 (square wave generator), channel 0, low/high byte access
    tty.Write("[PIT] Sending command 0x36\n");
    command_port.Write(0x36);
    
    // Send frequency divisor
    tty.Write("[PIT] Writing divisor bytes\n");
    data_port.Write(divisor & 0xFF);        // Low byte
    data_port.Write((divisor >> 8) & 0xFF); // High byte
    
    tty.Write("[PIT] PIT frequency set complete\n");
    Logger::Log("PIT frequency set");
}

// SchedulerTimerHandler implementation
SchedulerTimerHandler::SchedulerTimerHandler(InterruptManager* interrupt_manager, Scheduler* sched)
    : InterruptHandler(interrupt_manager, interrupt_manager->HardwareInterruptOffset() + 0), // IRQ0
      scheduler(sched), tick_count(0), frequency(100) // Default 100 Hz for good responsiveness
{
    // Set PIT frequency for scheduler timing
    PIT::SetFrequency(frequency);
    Logger::Log("Scheduler timer handler initialized");
}

uint32_t SchedulerTimerHandler::HandleInterrupt(uint32_t esp) {
    tick_count++;
    
    // Poll keyboard every tick to work around IRQ1 not firing in some environments
    // This is a fallback when keyboard interrupts don't work properly
    // Only poll if enabled (disabled during keyboard initialization to avoid race conditions)
    if (::kos::g_kbd_poll_enabled && ::kos::g_keyboard_driver_ptr) {
        // Poll up to 4 times per tick to drain queued data
        for (int i = 0; i < 4; ++i) {
            if (!::kos::g_keyboard_driver_ptr->PollOnce()) break;
        }
    }
    
    // Call scheduler's timer tick handler and get potentially new ESP
    if (scheduler) {
        esp = scheduler->OnTimerTick(esp);
    }
    
    // Return potentially modified ESP (if context switch occurred)
    return esp;
}