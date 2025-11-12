#pragma once
#ifndef  __KOS__CONSOLE__KEYBOARD_H
#define  __KOS__CONSOLE__KEYBOARD_H
#include <common/types.hpp>
#include <drivers/vga/vga.hpp>

using namespace kos::common;
using namespace kos::drivers;
using namespace kos::drivers::vga;  

namespace kos {
    namespace console {
        class TTY{
            public:
                TTY() = default;
                ~TTY() = default;
                static void Clear();
                static void Write(const int8_t* s);
                static void PutChar(const int8_t c);
                static void WriteHex(uint8_t key);
                // Color helpers
                static void SetColor(uint8_t fg, uint8_t bg);
                static void SetAttr(uint8_t a);
                // Discard any pre-initialization buffered output so a fresh
                // graphical terminal session can start without boot logs.
                static void DiscardPreinitBuffer();
            private:
                static VGA vga;
        };
    }
}




#endif