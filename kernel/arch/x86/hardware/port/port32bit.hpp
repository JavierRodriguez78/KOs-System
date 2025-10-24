#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PORT_PORT32BIT_H
#define __KOS__ARCH__X86__HARDWARE__PORT_PORT32BIT_H

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
                    *   @brief Represents a 32-bit hardware I/O port
                    */
                    class Port32Bit : public Port
                    {
                        
                        public:
                
                            /*
                            * @brief Constructs a Port32Bit object
                            * @param portnumber The port number to use
                            */
                            Port32Bit(uint16_t portnumber);
                
                            /*
                            * @brief Destroys a Port32Bit object
                            */
                            ~Port32Bit();

                
                            /*
                            * @brief Reads a 32-bit value from the port
                            * @return The 32-bit value read from the port
                            */  
                            virtual uint32_t Read();
                
                            /*
                            * @brief Writes a 32-bit value to the port
                            * @param data The 32-bit value to write to the port
                            */
                            virtual void Write(uint32_t data);

                        protected:

                            /*
                            * @brief Reads a 32-bit value from the port
                            * @param _port The port number to read from
                            * @return The 32-bit value read from the port
                            */
                            static inline uint32_t Read32(uint16_t _port)
                            {
                                uint32_t result;
                                __asm__ volatile("inl %1, %0" : "=a" (result) : "Nd" (_port));
                                return result;
                            }

                            /*
                            * @brief Writes a 32-bit value to the port
                            * @param _port The port number to write to
                            * @param _data The 32-bit value to write to the port
                            */
                            static inline void Write32(uint16_t _port, uint32_t _data)
                            {
                                __asm__ volatile("outl %0, %1" : : "a"(_data), "Nd" (_port));
                            }
                    };
                }
            }
        }
    }
}
#endif