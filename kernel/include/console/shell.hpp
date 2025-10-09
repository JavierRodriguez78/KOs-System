#ifndef  __KOS__CONSOLE__SHELL_H
#define  __KOS__CONSOLE__SHELL_H

#include <common/types.hpp>
#include <lib/libc.hpp>
#include <console/tty.hpp>
#include <drivers/keyboard.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::lib;

namespace kos{
    namespace console{
        class Shell {
        public:
            Shell();
            void Run(); // Main shell loop
            void InputChar(int8_t c); // Handle input from keyboard

        private:
            static const int32_t BUFFER_SIZE = 128;
            int8_t buffer[BUFFER_SIZE];
            int32_t bufferIndex;
            void PrintPrompt();
            void ExecuteCommand();
        };

        class ShellKeyboardHandler : public kos::drivers::KeyboardEventHandler{
            
            public:

                ShellKeyboardHandler();
                ~ShellKeyboardHandler();
                virtual void OnKeyDown(int8_t c);

            private:
            static TTY tty;
            static LibC LIBC;

        };
    }
}
#endif