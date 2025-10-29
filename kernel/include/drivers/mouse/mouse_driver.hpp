#pragma once
#ifndef  __KOS__DRIVERS__MOUSE_H
#define  __KOS__DRIVERS__MOUSE_H

#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <arch/x86/hardware/port/port.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>
#include <drivers/driver.hpp>
#include <drivers/mouse/mouse_event_handler.hpp>

using namespace kos::common;
using namespace kos::arch::x86::hardware::interrupts;
using namespace kos::arch::x86::hardware::port;
using namespace kos::drivers::mouse;

namespace kos{
    namespace drivers{
        namespace mouse{
        
            class MouseDriver:public InterruptHandler, public Driver
            {
                // I/O ports for mouse communication
                Port8Bit dataport;
                // Command port to send commands to the mouse
                Port8Bit commandport;
                // Buffer to store incoming mouse data
                uint8_t buffer[3];
                // Current offset in the buffer
                uint8_t offset;
                // Buttons currently pressed
                uint8_t buttons;
                // Mouse event handler
                MouseEventHandler* handler;
        
                public:
                    /*
                    @ brief Constructor for MouseDriver.
                    @ param manager Pointer to the InterruptManager.
                    @ param handler Pointer to the MouseEventHandler to handle mouse events.    
                    */
                    MouseDriver(InterruptManager* manager, MouseEventHandler* handler);
                    
                    /*
                    @ brief Destructor for MouseDriver.
                    */
                    ~MouseDriver();
                    
                    /*
                    @ brief Handle mouse interrupts.
                    @ param esp Stack pointer.
                    @ return Updated stack pointer.
                    */
                    virtual uint32_t HandleInterrupt(uint32_t esp);
                    
                    /*
                    @ brief Activate the mouse driver.
                    */  
                    virtual void Activate();
            };
        }
    }
}
#endif