#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PORT_PORT8BIT_H
#define __KOS__ARCH__X86__HARDWARE__PORT_PORT8BIT_H

#include <common/types.hpp>
#include "port.hpp"

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
                    *  @brief Represents an 8-bit hardware I/O port
                    */
                    class Port8Bit : public Port
                    {
            
                        public:
                
                            /*
                            * @brief Constructs a Port8Bit object
                            * @param portnumber The port number to use
                            */
                            Port8Bit(uint16_t portnumber);
                
                            /*
                            * @brief Destroys a Port8Bit object
                            */
                            ~Port8Bit();

                            /*
                            * @brief Writes an 8-bit value to the port
                            * @return The 8-bit value read from the port
                            */
                            virtual uint8_t Read();
                
                            /*
                            * @brief Reads an 8-bit value from the port
                            * @param data The 8-bit value to write to the port
                            */
                            virtual void Write(uint8_t data);

                        protected:
               
                            /*
                            * @brief Reads an 8-bit value from the port
                            * @return The 8-bit value read from the port
                            * @param _port The port number to read from
                            */
                            static inline uint8_t Read8(uint16_t _port)
                            {
                                uint8_t result;
                                __asm__ volatile("inb %1, %0" : "=a" (result) : "Nd" (_port));
                                return result;
                            }

                            
                            /*
                            * @brief Writes an 8-bit value to the port
                            * @param _port The port number to write to
                            * @param _data The 8-bit value to write to the port
                            */
                            static inline void Write8(uint16_t _port, uint8_t _data)
                            {
                                __asm__ volatile("outb %0, %1" : : "a" (_data), "Nd" (_port));
                            }
                    };

                }
            }
        }
    }
}

#endif  