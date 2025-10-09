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
            void ExecuteCommand(const int8_t* command); // New overload to execute given command
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


        struct CommandEntry{
            const int8_t* name;
            void (*entry)();
        };

        class CommandRegistry{
            public:
                CommandRegistry() = default;
                ~CommandRegistry() = default;
                static void (*Find(const int8_t* name))();
                // Initialize and register built-in commands
                static void Init();
            
            private:
                static void Register(const int8_t* name, void (*entry)());
               
                
        };
    }
}
#endif