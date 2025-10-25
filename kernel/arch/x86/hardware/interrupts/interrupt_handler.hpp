#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__INTERRUPTS__INTERRUPTHANDLER_H
#define __KOS__ARCH__X86__HARDWARE__INTERRUPTS__INTERRUPTHANDLER_H


#include <common/types.hpp>
#include <arch/x86/hardware/port/port.hpp>
#include <console/tty.hpp>

// Forward declaration to avoid circular include with interrupt_manager.hpp
namespace kos { 
    namespace arch { 
        namespace x86 { 
            namespace hardware { 
                namespace interrupts { 
                    class InterruptManager; 
                }
            }
        }
    }
}

using namespace kos::common;
using namespace kos::console;
using namespace kos::arch::x86::hardware::interrupts;

namespace kos
{
    namespace arch
    {
        namespace x86
        {
            namespace hardware
            {
                namespace interrupts
                {

                    /**
                    * @brief Base class for an individual interrupt handler
                    */
                    class InterruptHandler
                    {
                        protected:
                            // Interrupt number this handler manages
                            uint8_t InterruptNumber;
                            
                            // Pointer to the interrupt manager
                            InterruptManager* interruptManager;
                            
                            /**
                            * @brief Constructor: Registers this handler with the manager
                            * @param interruptManager Pointer to the InterruptManager
                            * @param InterruptNumber Interrupt number to handle
                            */
                            InterruptHandler(InterruptManager* interruptManager, uint8_t InterruptNumber);
                            
                            /**
                            * @brief Constructor: Registers this handler with the manager
                            * @param interruptManager Pointer to the InterruptManager
                            * @param InterruptNumber Interrupt number to handle
                            */
                            ~InterruptHandler();

                        public:

                            /**
                            * @brief Default handler, does nothing
                            * @param esp Stack pointer
                            * @return Same stack pointer
                            */
                            virtual uint32_t HandleInterrupt(uint32_t esp);
                    };
                }
            }
        }
    }
}

#endif