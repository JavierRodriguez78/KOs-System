#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PORT_PORT16BIT_H
#define __KOS__ARCH__X86__HARDWARE__PORT_PORT16BIT_H

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
                    * @brief Represents a 16-bit hardware I/O port
                    */
                    class Port16Bit : public Port
                    {
                        public:
                            /*
                            * @brief Constructs a Port16Bit object
                            * @param portnumber The port number to use
                            */
                            Port16Bit(uint16_t portnumber);

                            /*
                            * @brief Destroys a Port16Bit object
                            */
                            ~Port16Bit();

                            /*
                            * @brief Reads a 16-bit value from the port
                            * @return The 16-bit value read from the port
                            */
                            virtual uint16_t Read();
                            
                            /*
                            * @brief Writes a 16-bit value to the port
                            * @param data The 16-bit value to write to the port
                            */
                            virtual void Write(uint16_t data);

                        protected:
                
                            /*
                            * @brief Reads a 16-bit value from the port 
                            * @return The 16-bit value read from the port
                            * @param _port The port number to read from
                            */
                            static inline uint16_t Read16(uint16_t _port)
                            {
                                
                                uint16_t result;
                                __asm__ volatile("inw %1, %0" : "=a" (result) : "Nd" (_port));
                                return result;
                            }

                            /*
                            * @brief Writes a 16-bit value to the port
                            * @param _port The port number to write to
                            * @param _data The 16-bit value to write to the port
                            */
                            static inline void Write16(uint16_t _port, uint16_t _data)
                            {
                                __asm__ volatile("outw %0, %1" : : "a" (_data), "Nd" (_port));
                            }
                    };
 
 
                }
            }
        }
    }
}
#endif

