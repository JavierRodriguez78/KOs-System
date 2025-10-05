#ifndef  __KOS__CONSOLE__KEYBOARD_H
#define  __KOS__CONSOLE__KEYBOARD_H
#include <common/types.hpp>
#include <drivers/vga.hpp>

namespace kos {
    namespace console {
        class TTY{
            public:
                TTY();
                ~TTY();
                void Init();
                void Clear();
                void Write(const kos::common::int8_t* s);
                void PutChar(const kos::common::int8_t c);
                void WriteHex(kos::common::uint8_t key);
            private:
                kos::drivers::VGA vga;
        };
    }
}




#endif