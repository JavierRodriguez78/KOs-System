#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::lib;

namespace kos{
    namespace console{

        // Command registry implementation
        static const int32_t MAX_COMMANDS = 32;
        static CommandEntry commandTable[MAX_COMMANDS];
        static int32_t commandCount = 0;

        void CommandRegistry::Register(const int8_t* name, void (*entry)()) {
            if (commandCount < MAX_COMMANDS) {
                commandTable[commandCount].name = name;
                commandTable[commandCount].entry = entry;
                commandCount++;
            }
        }

        void (*CommandRegistry::Find(const int8_t* name))() {
            for (int32_t i = 0; i < commandCount; ++i) {
            
                if (String::strcmp(reinterpret_cast<const uint8_t*>(commandTable[i].name), reinterpret_cast<const uint8_t*>(name)) == 0) {
                    return commandTable[i].entry;
                }
            }
            return nullptr;
        }

        // --- Built-in commands for testing ---
        static void cmd_hello() {
            TTY tty;
            tty.Write("Hello from /Bin/hello!\n");
        }
        static void cmd_echo() {
            TTY tty;
            tty.Write("Echo command executed.\n");
        }

        void CommandRegistry::Init() {
            // Register a couple of built-in commands under /Bin/
            Register((const int8_t*)"hello", &cmd_hello);
            Register((const int8_t*)"echo", &cmd_echo);
        }

    }
}
