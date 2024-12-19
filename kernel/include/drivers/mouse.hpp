#ifndef  __KOS__DRIVERS__MOUSE_H
#define  __KOS__DRIVERS__MOUSE_H

#include <common/types.hpp>
#include <hardware/interrupts.hpp>
#include <hardware/port.hpp>
#include <drivers/driver.hpp>

namespace kos{
    namespace drivers{
        class MouseEventHandler
        {
        
            public:
                MouseEventHandler();

                virtual void OnActivate();
                virtual void OnMouseDown(kos::common::uint8_t button);
                virtual void OnMouseUp(kos::common::uint8_t button);
                virtual void OnMouseMove(int x, int y);
        };


        class MouseDriver:public kos::hardware::InterruptHandler, public Driver
        {
            kos::hardware::Port8Bit dataport;
            kos::hardware::Port8Bit commandport;

            kos::common::uint8_t buffer[3];
            kos::common::uint8_t offset;
            kos::common::uint8_t buttons;
            MouseEventHandler* handler;
        
        public:
            MouseDriver(kos::hardware::InterruptManager* manager, MouseEventHandler* handler);
            ~MouseDriver();
            virtual kos::common::uint32_t HandleInterrupt(kos::common::uint32_t esp);
            virtual void Activate();
        };
    }
}
#endif