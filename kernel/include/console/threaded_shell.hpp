#ifndef __KOS__CONSOLE__THREADED_SHELL_H
#define __KOS__CONSOLE__THREADED_SHELL_H

#include <common/types.hpp>
#include <process/sync.hpp>

using namespace kos::common;

namespace kos {
    namespace console {

    // Use process-level synchronization primitives

        static const uint32_t SHELL_BUFFER_SIZE = 256;

        // Threaded shell implementation
        class ThreadedShell {
        private:
            char input_buffer[SHELL_BUFFER_SIZE];
            uint32_t input_buffer_pos;
            uint32_t shell_thread_id;
            uint32_t command_count;
            
            kos::process::Mutex* shell_mutex;
            bool input_ready;
            
            // Internal methods
            void ShowWelcome();
            void MainLoop();
            void ShowPrompt();
            void WaitForInput();
            void ExecuteCommand();
            void ParseCommandLine(const char* cmdline, char* command, char* args);
            uint32_t CreateCommandThread(const char* command, const char* args);
            void* GetCommandEntryPoint(const char* command);
            
        public:
            ThreadedShell();
            ~ThreadedShell();
            
            // Shell management
            bool Initialize();
            void Start();
            
            // Input handling
            void OnKeyPressed(char key);
            
            // Status
            void GetStatus(uint32_t* total_commands, uint32_t* active_threads) const;
            uint32_t GetShellThreadId() const { return shell_thread_id; }
            uint32_t GetCommandCount() const { return command_count; }
        };

        // Global threaded shell instance
        extern ThreadedShell* g_threaded_shell;

        // Shell API
        namespace ThreadedShellAPI {
            bool InitializeShell();
            void StartShell();
            void ProcessKeyInput(char key);
            void ShowShellStatus();
        }

    } // namespace console
} // namespace kos

#endif // __KOS__CONSOLE__THREADED_SHELL_H