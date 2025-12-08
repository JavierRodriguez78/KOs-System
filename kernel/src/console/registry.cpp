#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>
#include <application/app.hpp>

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

        // Weak declarations for optional app_* commands.
        extern "C" void app_ls() __attribute__((weak));
        extern "C" void app_mkdir() __attribute__((weak));
        extern "C" void app_pwd() __attribute__((weak));
        extern "C" void app_clear() __attribute__((weak));
        extern "C" void app_cat() __attribute__((weak));
        extern "C" void app_reboot() __attribute__((weak));
        extern "C" void app_ifconfig() __attribute__((weak));


        void CommandRegistry::Init() {
            // Register built-in demo commands
            Register((const int8_t*)"hello", &cmd_hello);
            Register((const int8_t*)"echo", &cmd_echo);

            // Register embedded application commands if linked into kernel
            if (app_ls)      Register((const int8_t*)"ls",     app_ls);
            if (app_mkdir)   Register((const int8_t*)"mkdir",  app_mkdir);
            if (app_pwd)     Register((const int8_t*)"pwd",    app_pwd);
            if (app_clear)   Register((const int8_t*)"clear",  app_clear);
            if (app_cat)     Register((const int8_t*)"cat",    app_cat);
            if (app_reboot)  Register((const int8_t*)"reboot", app_reboot);
            if (app_ifconfig)Register((const int8_t*)"ifconfig", app_ifconfig);
            // Add more here as they are exposed via app.hpp
        }

    }
}
