#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PORT8BIT_H
#define __KOS__ARCH__X86__HARDWARE__PORT8BIT_H

#include <common/types.hpp>
#include "port8bit.hpp"

using namespace kos::common;
using namespace kos::arch::x86::hardware::port;

namespace kos
{
    namespace arch
    {
        namespace x86
        {
            namespace hardware
            {
                namespace port
                {
                    /*
                    * @brief Represents a slow 8-bit hardware I/O port
                    */  
                    class Port8BitSlow : public Port8Bit
                    {
                        
                        public:
                            /*
                            * @brief Constructs a Port8BitSlow object
                            * @param portnumber The port number to use
                            */
                            Port8BitSlow(uint16_t portnumber);

                            /*
                            * @brief Destroys a Port8BitSlow object
                            */
                            ~Port8BitSlow();
                
                            /*
                            * @brief Writes an 8-bit value to the port slowly
                            * @param data The 8-bit value to write to the port
                            */
                            virtual void Write(uint8_t data);

                        protected:
                            /*
                            * @brief Writes an 8-bit value to the port slowly
                            * @param _port The port number to write to
                            * @param _data The 8-bit value to write to the port
                            */
                            static inline void Write8Slow(uint16_t _port, uint8_t _data)
                            {
                                __asm__ volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" : : "a" (_data), "Nd" (_port));
                            }

                    };  
                }
            }
        }
    }
}

#endif // __KOS__ARCH__X86__HARDWARE__PORT8BIT_SLOW_H