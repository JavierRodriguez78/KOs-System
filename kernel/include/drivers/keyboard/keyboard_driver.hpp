#pragma once

#ifndef  __KOS__DRIVERS__KEYBOARD__KEYBOARD_DRIVER_H
#define  __KOS__DRIVERS__KEYBOARD__KEYBOARD_DRIVER_H

#include <common/types.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard/keyboard_handler.hpp>

using namespace kos::arch::x86::hardware::interrupts;

namespace kos
{
    namespace drivers
    {
        namespace keyboard
        {
            /*
            *@brief KeyboardDriver class is responsible for handling keyboard input.
            */
            class KeyboardDriver:public InterruptHandler, public Driver
            {
          
                public:
                    /*
                    *@brief Constructor for the KeyboardDriver class.
                    *@param manager Pointer to the InterruptManager
                    *@param handler Pointer to the KeyboardEventHandler
                    */
                    KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler);
                    
                    /*
                    *@brief Destructor for the KeyboardDriver class.
                    */
                    ~KeyboardDriver();
                    
                    /*
                    *@brief Handles keyboard interrupts.
                    *@param esp The current stack pointer
                    *@return The updated stack pointer
                    */
                    virtual uint32_t HandleInterrupt(uint32_t esp);
                    
                    /*
                    *@brief Activates the keyboard driver.
                    */
                    virtual void Activate();
                    // Poll one scancode (fallback when IRQ1 not firing). Returns true if a key was processed.
                    bool PollOnce();
                private:
                    
                    //  I/O ports for keyboard data and command
                    Port8Bit dataport;
                    
                    //  I/O port for keyboard command
                    Port8Bit commandport;
                    
                    //  Event handler for keyboard events
                    KeyboardEventHandler* handler;
                    
                    //  Console for logging keyboard activity
                    TTY tty;
                    // Tracks whether the previous byte was the extended 0xE0 prefix
                    bool e0Prefix = false;
                    // Modifier state
                    bool ctrlLeft = false;
                    bool ctrlRight = false;
            };
    
        }   // namespace keyboard
    }   // namespace drivers
}   // namespace kos

#endif