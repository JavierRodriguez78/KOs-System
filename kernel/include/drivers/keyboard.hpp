#ifndef  __KOS__DRIVERS__KEYBOARD_H
#define  __KOS__DRIVERS__KEYBOARD_H

#include <common/types.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/port.hpp>
#include <drivers/driver.hpp>
#include <console/tty.hpp>

using namespace kos::common;
using namespace kos::hardware;
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
        };
    
        class Keyboard{
            public:
                void WaitKey();
        };

        
    
    
    }


}


#endif