#pragma once

#ifndef  __KOS__DRIVERS__KEYBOARD__KEYBOARDHANDLER_H
#define  __KOS__DRIVERS__KEYBOARD__KEYBOARDHANDLER_H

#include <common/types.hpp>

using namespace kos::common;
namespace kos
{
    namespace drivers
    {
        namespace keyboard
        {
            /*
            *@brief Keyboard event handler interface
            */
            class KeyboardEventHandler
            {
                public:
                    
                    /*
                    *@brief Constructor for the keyboard event handler
                    */
                    KeyboardEventHandler();
                    
                    /*
                    *@brief Called when a key is pressed down
                    *@param c The character of the key pressed
                    */
                    virtual void OnKeyDown(int8_t);
                    /*
                    *@brief Called when a key is released
                    *@param c The character of the key released
                    */
                    virtual void OnKeyUp(int8_t);
            };
        }   // namespace keyboard
    }
}

#endif