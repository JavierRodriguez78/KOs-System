#ifndef  __KOS__DRIVERS__MOUSE_H
#define  __KOS__DRIVERS__MOUSE_H

#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <hardware/port.hpp>
#include <drivers/driver.hpp>

using namespace kos::common;
using namespace kos::hardware;
using namespace kos::arch::x86::hardware::interrupts;

namespace kos{
    namespace drivers{
        class MouseEventHandler
        {
        
            public:
                MouseEventHandler();

                virtual void OnActivate();
                virtual void OnMouseDown(uint8_t button);
                virtual void OnMouseUp(uint8_t button);
                virtual void OnMouseMove(int32_t x, int32_t y);
        };


        class MouseDriver:public InterruptHandler, public Driver
        {
            Port8Bit dataport;
            Port8Bit commandport;

            uint8_t buffer[3];
            uint8_t offset;
            uint8_t buttons;
            MouseEventHandler* handler;
        
        public:
            MouseDriver(InterruptManager* manager, MouseEventHandler* handler);
            ~MouseDriver();
            virtual uint32_t HandleInterrupt(uint32_t esp);
            virtual void Activate();
        };
    }
}
#endif