#pragma once

#ifndef  __KOS__DRIVERS__KEYBOARD__KEYBOARD_H
#define  __KOS__DRIVERS__KEYBOARD__KEYBOARD_H


#include <arch/x86/hardware/port/port.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>

#include <console/tty.hpp>

using namespace kos::common;
using namespace kos::arch::x86::hardware::port;
using namespace kos::console;

namespace kos
{
    namespace drivers
    {
        namespace keyboard
        {
            /*
            * @brief Keyboard class is responsible for handling keyboard input.
            */
            class Keyboard{
                public:
                    /*
                    * @brief Waits for a key press.
                    */
                    void WaitKey();
            };
        }
    }
}


#endif