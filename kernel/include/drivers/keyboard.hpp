#ifndef  __KOS__DRIVERS__KEYBOARD_H
#define  __KOS__DRIVERS__KEYBOARD_H

#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <arch/x86/hardware/port/port.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>
#include <drivers/driver.hpp>
#include <console/tty.hpp>

using namespace kos::common;
using namespace kos::arch::x86::hardware::port;
using namespace kos::arch::x86::hardware::interrupts;
using namespace kos::console;

namespace kos
{
    namespace drivers
    {
        class KeyboardEventHandler
        {
            public:
                KeyboardEventHandler();

                virtual void OnKeyDown(int8_t);
                virtual void OnKeyUp(int8_t);
        };

        class KeyboardDriver:public InterruptHandler, public Driver
        {
          
        public:
            KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler);
            ~KeyboardDriver();
            virtual uint32_t HandleInterrupt(uint32_t esp);
            virtual void Activate();
        private:
            Port8Bit dataport;
            Port8Bit commandport;
            KeyboardEventHandler* handler;
            TTY tty;
            // Tracks whether the previous byte was the extended 0xE0 prefix
            bool e0Prefix = false;
        };
    
        class Keyboard{
            public:
                void WaitKey();
        };
    }
}


#endif