#pragma once
#ifndef  __KOS__DRIVERS__MOUSE__MOUSE_EVENT_HANDLER_H
#define  __KOS__DRIVERS__MOUSE__MOUSE_EVENT_HANDLER_H

#include <common/types.hpp>
using namespace kos::common;

namespace kos{
    namespace drivers{
        namespace mouse{
            /*
            *@brief Abstract base class for handling mouse events.
            *@details Derive from this class and override the virtual methods to handle mouse events such as
            */
            class MouseEventHandler
            {
                public:
                    /*
                    * @brief Constructor for MouseEventHandler.
                    */
                    MouseEventHandler();
                    
                    /*
                    * @brief Called when the mouse is activated.
                    */
                    virtual void OnActivate();

                    /*
                    * @brief Called when a mouse button is pressed.
                    */
                    virtual void OnMouseDown(uint8_t button);
                    
                    /*
                    * @brief Called when a mouse button is released.
                    */
                    virtual void OnMouseUp(uint8_t button);
                    
                    /*
                    * @brief Called when the mouse is moved.
                    */
                    virtual void OnMouseMove(int32_t x, int32_t y);
            };
        }
    }
}


#endif // __KOS__DRIVERS__MOUSE__MOUSE_EVENT_HANDLER_H