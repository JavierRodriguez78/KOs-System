#ifndef  __KOS__CONSOLE__KEYBOARD_H
#define  __KOS__CONSOLE__KEYBOARD_H
#include <common/types.hpp>
#include <drivers/vga.hpp>

namespace kos {
    namespace console {
        class TTY{
            public:
                TTY() = default;
                ~TTY() = default;
                static void Clear();
                static void Write(const kos::common::int8_t* s);
                static void PutChar(const kos::common::int8_t c);
                static void WriteHex(kos::common::uint8_t key);
            private:
                static kos::drivers::VGA vga;
        };
    }
}




#endif