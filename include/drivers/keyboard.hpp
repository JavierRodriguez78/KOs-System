#ifndef  __KOS__DRIVERS__KEYBOARD_H
#define  __KOS__DRIVERS__KEYBOARD_H

#include <common/types.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/port.hpp>
#include <drivers/driver.hpp>

namespace kos
{
    namespace drivers
    {
        class KeyboardEventHandler
        {
            public:
                KeyboardEventHandler();

                virtual void OnKeyDown(char);
                virtual void OnKeyUp(char);
        };

        class KeyboardDriver:public kos::hardware::InterruptHandler, public Driver
        {
            kos::hardware::Port8Bit dataport;
            kos::hardware::Port8Bit commandport;
            KeyboardEventHandler* handler;
        public:
            KeyboardDriver(kos::hardware::InterruptManager* manager, KeyboardEventHandler *handler);
            ~KeyboardDriver();
            virtual kos::common::uint32_t HandleInterrupt(kos::common::uint32_t esp);
            virtual void Activate();
        };
    }
}


#endif