#ifndef __KOS__DRIVERS__VGA_H
#define __KOS__DRIVERS__VGA_H

#include <common/types.hpp>

using namespace kos::common;

namespace kos{
    namespace drivers{
        
        /**
        * @class VGA
        * @brief Basic VGA text mode driver for writing characters to the screen.
        */
        class VGA{

            private: 
                // Pointer to VGA text buffer at 0xB8000
                static uint16_t* VideoMemory; 
                // Current cursor position (x,y) and text attribute
                static uint8_t x,y;
                // Text attribute (fg|bg)
                static uint8_t attr; 
            public:
                VGA();
                ~VGA();
            
                /**
                * @brief Initialises the VGA controller.
                * @details Clears the screen and places the cursor at (0,0).
                */
                void Init();

                /**
                 * @brief Clears the screen by filling it with blank characters.
                 * @details Fills the framebuffer with spaces and the current colour.
                 */
                void Clear();

                /**
                * @brief Writes a character at the current position.
                * @param c ASCII character to be written.
                * @details
                * - Supports line breaks (`“\n”`).
                * - Automatically scrolls if the last line is exceeded.
                */
                void PutChar(int8_t c);
                
                /**
                * @brief Writes a text string terminated by “\0”.
                * @param str C string to be printed.
                */
                void Write(const int8_t* str);

                /**
                * @brief Writes a hexadecimal value.
                * @param key 8-bit value to be printed.
                */
                void WriteHex(uint8_t key);
                
                /**
                * @brief Sets the foreground and background colors.
                * @param fg Foreground color (0-15).
                * @param bg Background color (0-15).
                * @details Colors are specified using standard VGA color codes.
                */
                void SetColor(uint8_t fg, uint8_t bg);

                /**
                * @brief Sets the text attribute.
                * @param a Attribute byte (foreground and background color).
                */
                void SetAttr(uint8_t a);

                /**
                * @brief Combines foreground and background colors into an attribute byte.
                * @param fg Foreground color (0-15).
                * @param bg Background color (0-15).
                * @return Combined attribute byte.
                */
                static inline uint8_t MakeAttr(uint8_t fg, uint8_t bg) {
                    return ((bg & 0x0F) << 4) | (fg & 0x0F);
                }
        };

    }
}

#endif