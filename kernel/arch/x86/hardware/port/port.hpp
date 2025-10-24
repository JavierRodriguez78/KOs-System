#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PORT_H
#define __KOS__ARCH__X86__HARDWARE__PORT_H

#include <common/types.hpp>
// Bring well-known x86 I/O port constants into scope for consumers of this header
// without forcing them to know the arch-specific path.
// The constants live alongside this header.
#include "port_constants.hpp"

using namespace kos::common;    

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
                    
                    // --------------------------------------------------
                    // Convenience alias so users including this header can access constants as 
                    // kos::hardware::portc::PORT_KEYBOARD_DATA, etc., while keeping the
                    // constants defined in their original architecture-specific namespace.
                    namespace portc = kos::arch::x86::hardware::port;
            
                    /*
                    *  @brief Represents a hardware I/O port
                    *  Convenience alias so users including this header can access constants as 
                    *  kos::hardware::portc::PORT_KEYBOARD_DATA, etc., while keeping the
                    *  constants defined in their original architecture-specific namespace.
                    */
                    class Port
                    {
            
                        protected:
                            /*
                            *@brief Constructs a Port object
                            *@param portnumber The port number to use
                            */
                            Port(uint16_t portnumber);
                
                            /*
                            *@brief Destroys a Port object
                            */
                            ~Port();
                
                            // The port number associated with this Port instance
                            uint16_t portnumber;
                    };
                } // namespace port
            } // namespace hardware
        } // namespace x86
    } // namespace arch
}
#endif