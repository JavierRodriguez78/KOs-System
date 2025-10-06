#ifndef  __KOS__CONSOLE__SHELL_H
#define  __KOS__CONSOLE__SHELL_H

#include<common/types.hpp>
#include<lib/libc.hpp>
#include<console/tty.hpp>
#include<drivers/keyboard.hpp>

namespace kos{
    namespace console{
        class Shell{
            public:
                Shell();
                ~Shell();
                void Exec(const kos::common::uint8_t* cmd);
                void Run();
            private:
                kos::lib::LibC LIBC;
                static kos::console::TTY TTY;
        };

        class ShellKeyboardHandler : public kos::drivers::KeyboardEventHandler{
            
            public:

                ShellKeyboardHandler();
                ~ShellKeyboardHandler();
                virtual void OnKeyDown(char c);

            private:
            static kos::console::TTY tty;

        };
    }
}
#endif