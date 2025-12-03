
#include <console/shell.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>
#include <drivers/keyboard/keyboard.hpp>
#include <fs/filesystem.hpp>
#include <lib/elfloader.hpp>
// sys API utilities are declared in stdio.hpp
#include <lib/stdio.hpp>
// Logo printer
#include <console/logo.hpp>
#include <graphics/framebuffer.hpp>
#include <ui/cursor.hpp>
#include <kernel/globals.hpp>
#include <drivers/mouse/mouse_driver.hpp>
// Scheduler / threads
#include <process/scheduler.hpp>
// Pipe management
#include <process/pipe.hpp>
#include <services/user_service.hpp>
#include <services/service_manager.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::drivers;
using namespace kos::sys;
using namespace kos::process;

// File-local TTY instance for output
static TTY tty;

// Local reboot helper (avoids kernel link on app_reboot)
static inline void io_outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t io_inb(uint16_t port) {
    uint8_t ret; __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static void shell_reboot_hw() {
    // Wait until KBC input buffer empty
    while (io_inb(0x64) & 0x02) { }
    io_outb(0x64, 0xFE);
    // Fallback: try triple-fault by loading null IDT
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ __volatile__("lidt %0\n\tint $0x03" : : "m"(null_idt));
    // Halt if still running
    for(;;) { __asm__ __volatile__("hlt"); }
}

// Optional filesystem access from shell
namespace kos { 
    namespace fs { 
        class Filesystem; 
    } 
}

// Use fully qualified name for global filesystem pointer

// (logo printer now lives in src/console/logo.cpp)

Shell::Shell() : bufferIndex(0) {
    for (int32_t i = 0; i < BUFFER_SIZE; ++i) buffer[i] = 0;
    mode = Mode::LoginUser;
    loginUserLen = 0;
}

void Shell::PrintPrompt() {
    const int8_t* cwd = kos::sys::table()->cwd ? kos::sys::table()->cwd : (const int8_t*)"/";
    // Prompt prefix: user@kos:
    const auto* usvc = kos::services::GetUserService();
    const int8_t* uname = usvc ? usvc->CurrentUserName() : (const int8_t*)"user";
    bool isRoot = usvc ? usvc->IsSuperuser() : false;
    tty.Write(uname);
    tty.Write((const int8_t*)"@kos:");

    // Print path with basename colorized (closest VGA color to #27F5E7 is light cyan = 11)
    // Find last '/'
    const int8_t* last_slash = nullptr;
    for (const int8_t* p = cwd; *p; ++p) {
        if (*p == '/') last_slash = p;
    }
    if (!last_slash) {
        // No slash found, colorize entire cwd
        tty.SetColor(11, 0);
        tty.Write(cwd);
        tty.SetAttr(0x07);
    } else {
        // Write prefix up to and including last '/'
        // Compute length from cwd start to last_slash inclusive
        int32_t prefix_len = (int32_t)(last_slash - cwd + 1);
        // Write prefix normally
        for (int32_t i = 0; i < prefix_len; ++i) tty.PutChar(cwd[i]);
        // Write basename in color
        const int8_t* base = last_slash + 1;
        if (*base) {
            tty.SetColor(11, 0);
            tty.Write(base);
            tty.SetAttr(0x07);
        }
    }

    tty.Write(isRoot ? (const int8_t*)"# " : (const int8_t*)"$ ");
}

void Shell::Run() {
 
    if (kos::gfx::IsAvailable()) {
        PrintLogoFramebuffer32();
    } else {
        PrintLogoBlockArt();
    }
    // Interactive login prompt before normal shell
    tty.Write("KOS login: ");
    // Initialize current working directory to filesystem root
    // Regardless of filesystem presence, use "/" as the starting directory
    SetCwd((const int8_t*)"/");
    // Welcome will be printed after successful login
    while (true) {
        // In a real kernel, input would come from keyboard interrupts
        // Here, input is handled via InputChar() called by the keyboard handler
    }
}

void Shell::InputChar(int8_t c) {
    // Normalize CR to LF and handle Enter immediately
    if (c == '\r') c = '\n';
    // Handle login modes separately
    if (mode != Mode::Normal) {
        // Enforce timeout lock if set
        uint32_t now = kos::services::ServiceManager::UptimeMs();
        if (lock_until_ms != 0 && now != 0 && now < lock_until_ms) {
            return; // ignore input until timeout elapses
        }
        lock_until_ms = 0;
        if (mode == Mode::LoginUser) {
            if (c == '\n') {
                loginUser[loginUserLen] = 0;
                tty.Write("\nPassword: ");
                mode = Mode::LoginPass;
                bufferIndex = 0;
                return;
            } else if (c == '\b' || c == 127) {
                if (loginUserLen > 0) { loginUserLen--; tty.Write("\b \b"); }
                return;
            } else if (c >= 32 && loginUserLen < (int)sizeof(loginUser)-1) {
                loginUser[loginUserLen++] = c;
                tty.PutChar(c);
                return;
            } else {
                return;
            }
        } else if (mode == Mode::LoginPass) {
            if (c == '\n') {
                buffer[bufferIndex] = 0;
                auto* usvc = kos::services::GetUserService();
                bool ok = (usvc && usvc->Login(loginUser, buffer));
                tty.Write("\n");
                if (ok) {
                    tty.Write("Welcome to KOS Shell\n");
                    mode = Mode::Normal;
                    PrintPrompt();
                } else {
                    tty.Write("Login incorrect\n");
                    // Reset and prompt again
                    loginUserLen = 0;
                    bufferIndex = 0;
                    if (--retries_left <= 0) {
                        uint32_t now2 = kos::services::ServiceManager::UptimeMs();
                        lock_until_ms = (now2 ? now2 : 0) + 3000; // 3s lock
                        retries_left = 3;
                        tty.Write("Too many attempts. Try again in 3s.\n");
                    }
                    mode = Mode::LoginUser;
                    tty.Write("KOS login: ");
                }
                return;
            } else if (c == '\b' || c == 127) {
                if (bufferIndex > 0) { bufferIndex--; tty.Write("\b \b"); }
                return;
            } else if (c >= 32 && bufferIndex < BUFFER_SIZE - 1) {
                // mask password input
                buffer[bufferIndex++] = c;
                tty.PutChar('*');
                return;
            } else {
                return;
            }
        }
    }
    if (c == '\n') {
        tty.PutChar('\n');
        buffer[bufferIndex] = 0;
        ExecuteCommand();
        bufferIndex = 0;
        PrintPrompt();
        return;
    } else if (c == '\b' || c == 127) { // Backspace
        if (bufferIndex > 0) {
            bufferIndex--;
            tty.Write("\b \b");
        }
        return;
    }

    // Handle control characters (Ctrl+[A-Z] -> 1..26). Other controls ignored.
    if (c > 0 && c < 32) {
        switch (c) {
            case 3: // Ctrl+C: cancel current line
                tty.Write("^C\n");
                bufferIndex = 0;
                PrintPrompt();
                return;
            case 12: // Ctrl+L: clear screen
                tty.Clear();
                PrintPrompt();
                return;
            case 21: // Ctrl+U: kill line
                while (bufferIndex > 0) {
                    bufferIndex--;
                    tty.Write("\b \b");
                }
                return;
            default:
                // Ignore other control chars in shell (do not echo)
                return;
        }
    } else if (bufferIndex < BUFFER_SIZE - 1) {
        buffer[bufferIndex++] = c;
        tty.PutChar(c);
    }
}

void Shell::ExecuteCommand() {
    if (bufferIndex == 0) return;
    ExecuteCommand(buffer);
}

void Shell::ExecuteCommand(const int8_t* command) {
    if (command == nullptr) return;
    if (*command == 0) return;

        // Parse program name and arguments from command line without mutating original buffer
        // Make a local mutable copy to split tokens
        int8_t localBuf[BUFFER_SIZE];
        int iCopy = 0;
        for (; command[iCopy] && iCopy < BUFFER_SIZE - 1; ++iCopy) localBuf[iCopy] = command[iCopy];
        localBuf[iCopy] = 0;

        const int32_t MAX_ARGS = 16;
        const int8_t* argv[MAX_ARGS];
        int32_t argc = 0;
        int8_t* p = localBuf;
        while (*p == ' ') ++p;
        while (*p && argc < MAX_ARGS) {
            argv[argc++] = p;
            while (*p && *p != ' ') ++p;
            if (!*p) break;
            *p++ = 0; // terminate token
            while (*p == ' ') ++p; // skip spaces before next token
    }
    if (argc == 0) return;
    const int8_t* prog = argv[0];

    // Built-in alias: cls -> clear application
    if (String::strcmp(prog, (const int8_t*)"cls", 3) == 0 && (prog[3] == 0)) {
        // Rewrite prog to "clear" and fall through to app execution
        prog = (const int8_t*)"clear";
    }



    // Built-in: show logo (text mode)
    if (String::strcmp(prog, (const int8_t*)"logo", 4) == 0 &&
        (prog[4] == 0)) {
        tty.Write("Executing logo command...\n");
        PrintLogoBlockArt();
        tty.Write("Logo command completed.\n");
        return;
    }

    // Built-in: show logo on framebuffer (32bpp)
    if (String::strcmp(prog, (const int8_t*)"logo32", 6) == 0 &&
        (prog[6] == 0)) {
        tty.Write("Executing logo32 command...\n");
        if (gfx::IsAvailable()) {
            tty.Write("Framebuffer available, showing 32bpp logo.\n");
            PrintLogoFramebuffer32();
        } else {
            tty.Write("Framebuffer 32bpp not available, falling back to text logo\n");
            PrintLogoBlockArt();
        }
        tty.Write("Logo32 command completed.\n");
        return;
    }

    // Built-in: gfxinfo (print framebuffer information)
    if (String::strcmp(prog, (const int8_t*)"gfxinfo", 7) == 0 &&
        (prog[7] == 0)) {
        if (gfx::IsAvailable()) {
            const auto& fb = gfx::GetInfo();
            tty.Write("Framebuffer: ");
            tty.Write("addr=0x");
            // Print high then low for 64-bit addr readability (simple hex pairs)
            uint64_t a = fb.addr;
            for (int i = 7; i >= 0; --i) {
                uint8_t byte = (a >> (i*8)) & 0xFF;
                tty.WriteHex(byte);
            }
            tty.Write(" pitch=");
            tty.WriteHex((fb.pitch >> 8) & 0xFF); tty.WriteHex(fb.pitch & 0xFF);
            tty.Write(" size=");
            tty.WriteHex((fb.width >> 8) & 0xFF); tty.WriteHex(fb.width & 0xFF);
            tty.Write("x");
            tty.WriteHex((fb.height >> 8) & 0xFF); tty.WriteHex(fb.height & 0xFF);
            tty.Write("x");
            tty.WriteHex(fb.bpp);
            tty.Write(" type=");
            tty.WriteHex(fb.type);
            tty.Write("\n");
        } else {
            tty.Write("Framebuffer not available. Reboot using the 'graphic mode' GRUB entry.\n");
        }
        return;
    }

    // Built-in: gfxinit (clear framebuffer, draw logo) and optional gfxclear <RRGGBB>
    if (String::strcmp(prog, (const int8_t*)"gfxinit", 7) == 0 && (prog[7] == 0)) {
        if (!gfx::IsAvailable()) {
            tty.Write("Framebuffer not available. Reboot using the 'graphic mode' GRUB entry.\n");
            return;
        }
        // Clear to black and draw logo
        gfx::Clear32(0xFF000000u);
        PrintLogoFramebuffer32();
        tty.Write("Graphics initialized.\n");
        return;
    }

    // Built-in: cursor [crosshair|triangle] (query or set cursor style)
    if (String::strcmp(prog, (const int8_t*)"cursor", 6) == 0 && (prog[6] == 0)) {
        if (!gfx::IsAvailable()) {
            tty.Write("Graphics mode not active; no cursor.\n");
            return;
        }
        if (argc == 1) {
            kos::ui::CursorStyle cs = kos::ui::GetCursorStyle();
            tty.Write("Cursor style: ");
            if (cs == kos::ui::CursorStyle::Crosshair) tty.Write("crosshair\n"); else tty.Write("triangle\n");
            return;
        }
        const int8_t* arg = argv[1];
        if (String::strcmp(arg, (const int8_t*)"crosshair", 9) == 0 && arg[9] == 0) {
            kos::ui::SetCursorStyle(kos::ui::CursorStyle::Crosshair);
            tty.Write("Cursor style set to crosshair\n");
            return;
        } else if (String::strcmp(arg, (const int8_t*)"triangle", 8) == 0 && arg[8] == 0) {
            kos::ui::SetCursorStyle(kos::ui::CursorStyle::Triangle);
            tty.Write("Cursor style set to triangle\n");
            return;
        } else {
            tty.Write("Usage: cursor [crosshair|triangle]\n");
            return;
        }
    }

    // Built-in: mousedbg [on|off] - dump raw mouse bytes (limited)
    if (String::strcmp(prog, (const int8_t*)"mousedbg", 8) == 0 && (prog[8] == 0)) {
        if (argc == 1) {
            tty.Write("Usage: mousedbg [on|off]\n");
            return;
        }
        // Use kernel global pointer (namespace ::kos)
        if (!::kos::g_mouse_driver_ptr) { tty.Write("Mouse driver not initialized\n"); return; }
        const int8_t* arg = argv[1];
        if (String::strcmp(arg, (const int8_t*)"on", 2) == 0 && arg[2] == 0) {
            ::kos::g_mouse_driver_ptr->EnableDebugDump(true);
            tty.Write("Mouse debug dump enabled (prints ~96 bytes)\n");
        } else if (String::strcmp(arg, (const int8_t*)"off", 3) == 0 && arg[3] == 0) {
            ::kos::g_mouse_driver_ptr->EnableDebugDump(false);
            tty.Write("Mouse debug dump disabled\n");
        } else {
            tty.Write("Usage: mousedbg [on|off]\n");
        }
        return;
    }

    if (String::strcmp(prog, (const int8_t*)"gfxclear", 8) == 0 && (prog[8] == 0)) {
        if (!gfx::IsAvailable()) {
            tty.Write("Framebuffer not available.\n");
            return;
        }
        // Optional argument: RRGGBB (hex)
        uint32_t rgba = 0xFF000000u; // default black
        if (argc >= 2) {
            const int8_t* s = argv[1];
            // Skip leading 0x if present
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
            uint32_t val = 0;
            for (int i = 0; s[i]; ++i) {
                int8_t ch = s[i];
                uint8_t v;
                if (ch >= '0' && ch <= '9') v = ch - '0';
                else if (ch >= 'a' && ch <= 'f') v = 10 + (ch - 'a');
                else if (ch >= 'A' && ch <= 'F') v = 10 + (ch - 'A');
                else break;
                val = (val << 4) | v;
            }
            // Interpret as RRGGBB
            if (val <= 0xFFFFFFu) rgba = 0xFF000000u | val;
        }
        gfx::Clear32(rgba);
        tty.Write("Framebuffer cleared.\n");
        return;
    }



    // Built-in: show scheduler statistics
    if (String::strcmp(prog, (const int8_t*)"ps", 2) == 0 &&
        (prog[2] == 0)) {
        if (!g_scheduler) {
            tty.Write("Scheduler not initialized\n");
        } else {
            tty.Write("Scheduler Statistics:\n");
            tty.Write("  Current task ID: ");
            tty.WriteHex(SchedulerAPI::GetCurrentThreadId());
            tty.Write("\n");

            tty.Write("  Total tasks: ");
            tty.WriteHex(g_scheduler->GetTaskCount());
            tty.Write("\n");

            tty.Write("  Scheduling enabled: ");
            tty.Write(g_scheduler->IsSchedulingEnabled() ? "Yes" : "No");
            tty.Write("\n");
        }
        return;
    }

    // Built-in: whoami
    if (String::strcmp(prog, (const int8_t*)"whoami", 6) == 0 && (prog[6] == 0)) {
        const auto* usvc = kos::services::GetUserService();
        const int8_t* uname = usvc ? usvc->CurrentUserName() : (const int8_t*)"(none)";
        tty.Write(uname);
        tty.PutChar('\n');
        return;
    }

    // Built-in: su <user> <password>
    if (String::strcmp(prog, (const int8_t*)"su", 2) == 0 && (prog[2] == 0)) {
        if (argc < 3) { tty.Write("Usage: su <user> <password>\n"); return; }
        auto* usvc = kos::services::GetUserService();
        if (!usvc) { tty.Write("UserService not available\n"); return; }
        if (usvc->Login(argv[1], argv[2])) {
            tty.Write("Login successful\n");
        } else {
            tty.Write("Authentication failed\n");
        }
        return;
    }

    // Built-in: adduser <user> <password> (root only)
    if (String::strcmp(prog, (const int8_t*)"adduser", 7) == 0 && (prog[7] == 0)) {
        auto* usvc = kos::services::GetUserService();
        if (!usvc) { tty.Write("UserService not available\n"); return; }
        if (!usvc->IsSuperuser()) { tty.Write("adduser: permission denied\n"); return; }
        if (argc < 3) { tty.Write("Usage: adduser <user> <password>\n"); return; }
        if (usvc->AddUser(argv[1], argv[2], false)) tty.Write("User added\n"); else tty.Write("Failed to add user\n");
        return;
    }

    // Built-in: passwd <user> <newpass> (root or self)
    if (String::strcmp(prog, (const int8_t*)"passwd", 6) == 0 && (prog[6] == 0)) {
        auto* usvc = kos::services::GetUserService();
        if (!usvc) { tty.Write("UserService not available\n"); return; }
        if (argc < 3) { tty.Write("Usage: passwd <user> <newpass>\n"); return; }
        const int8_t* target = argv[1];
        const int8_t* curr = usvc->CurrentUserName();
        bool allowed = usvc->IsSuperuser() || (String::strcmp(target, curr) == 0);
        if (!allowed) { tty.Write("passwd: permission denied\n"); return; }
        if (usvc->SetPassword(target, argv[2])) tty.Write("Password updated\n"); else tty.Write("Failed to update password\n");
        return;
    }

    // Built-in: logout - return to login prompt
    if (String::strcmp(prog, (const int8_t*)"logout", 6) == 0 && (prog[6] == 0)) {
        auto* usvc = kos::services::GetUserService();
        if (usvc) usvc->Logout();
        // reset shell to login mode
        mode = Mode::LoginUser;
        loginUserLen = 0;
        bufferIndex = 0;
        retries_left = 3;
        lock_until_ms = 0;
        tty.Write("Logged out.\n");
        tty.Write("KOS login: ");
        return;
    }

    // Privilege guard: reboot/shutdown require root
    if ((String::strcmp(prog, (const int8_t*)"reboot", 6) == 0 && (prog[6] == 0)) ||
        (String::strcmp(prog, (const int8_t*)"shutdown", 8) == 0 && (prog[8] == 0))) {
        const auto* usvc = kos::services::GetUserService();
        if (!usvc || !usvc->IsSuperuser()) {
            tty.Write("permission denied (root required)\n");
            return;
        }
        // fallthrough: allow execution as external app
    }

    // Built-in: detailed thread list
    if (String::strcmp(prog, (const int8_t*)"threads", 7) == 0 &&
        (prog[7] == 0)) {
        if (g_scheduler) {
            g_scheduler->PrintTaskList();
        } else {
            tty.Write("Scheduler not initialized\n");
        }
        return;
    }

    // Built-in: kill thread
    if (String::strcmp(prog, (const int8_t*)"kill", 4) == 0 &&
        (prog[4] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }
        
        if (SchedulerAPI::KillThread(thread_id)) {
            tty.Write("Thread killed successfully\n");
        } else {
            tty.Write("Failed to kill thread\n");
        }
        return;
    }

    // Built-in: suspend thread
    if (String::strcmp(prog, (const int8_t*)"suspend", 7) == 0 &&
        (prog[7] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }
        
        if (SchedulerAPI::SuspendThread(thread_id)) {
            tty.Write("Thread suspended successfully\n");
        } else {
            tty.Write("Failed to suspend thread\n");
        }
        return;
    }

    // Built-in: resume thread
    if (String::strcmp(prog, (const int8_t*)"resume", 6) == 0 &&
        (prog[6] == 0) && argc >= 2) {
        uint32_t thread_id = 0;
        // Simple hex parsing for thread ID
        const int8_t* id_str = argv[1];
        for (int i = 0; id_str[i]; i++) {
            thread_id <<= 4;
            if (id_str[i] >= '0' && id_str[i] <= '9') {
                thread_id += id_str[i] - '0';
            } else if (id_str[i] >= 'a' && id_str[i] <= 'f') {
                thread_id += id_str[i] - 'a' + 10;
            } else if (id_str[i] >= 'A' && id_str[i] <= 'F') {
                thread_id += id_str[i] - 'A' + 10;
            }
        }

        if (SchedulerAPI::ResumeThread(thread_id)) {
            tty.Write("Thread resumed successfully\n");
        } else {
            tty.Write("Failed to resume thread\n");
        }
        return;
    }

    // Built-in: sleep current thread
    if (String::strcmp(prog, (const int8_t*)"sleep", 5) == 0 &&
        (prog[5] == 0) && argc >= 2) {
        uint32_t ms = 0;
        // Simple decimal parsing for milliseconds
        const int8_t* ms_str = argv[1];
        for (int i = 0; ms_str[i]; i++) {
            if (ms_str[i] >= '0' && ms_str[i] <= '9') {
                ms = ms * 10 + (ms_str[i] - '0');
            }
        }
        
        tty.Write("Sleeping for ");
        tty.WriteHex(ms);
        tty.Write(" ms...\n");
        SchedulerAPI::SleepThread(ms);
        return;
    }

    // Built-in: yield current task (if any)
    if (String::strcmp(prog, (const int8_t*)"yield", 5) == 0 &&
        (prog[5] == 0)) {
        tty.Write("Yielding CPU...\n");
        SchedulerAPI::YieldThread();
        return;
    }

    // Built-in: pipes command (show all pipes)
    if (String::strcmp(prog, (const int8_t*)"pipes", 5) == 0 &&
        (prog[5] == 0)) {
        if (g_pipe_manager) {
            g_pipe_manager->PrintAllPipes();
        } else {
            tty.Write("Pipe manager not initialized\n");
        }
        return;
    }

    // Built-in: mkpipe command (create pipe)
    if (String::strcmp(prog, (const int8_t*)"mkpipe", 6) == 0 &&
        (prog[6] == ' ' || prog[6] == 0)) {
        if (prog[6] == 0) {
            tty.Write("Usage: mkpipe <name> [buffer_size]\n");
            return;
        }
        
        // Find pipe name
        const int8_t* name_start = prog + 7;
        while (*name_start == ' ') name_start++; // Skip spaces
        
        if (*name_start == 0) {
            tty.Write("Usage: mkpipe <name> [buffer_size]\n");
            return;
        }
        
        // Find end of name
        const int8_t* name_end = name_start;
        while (*name_end && *name_end != ' ') name_end++;
        
        // Extract name
        char pipe_name[32];
        int name_len = name_end - name_start;
        if (name_len >= sizeof(pipe_name)) name_len = sizeof(pipe_name) - 1;
        for (int i = 0; i < name_len; i++) {
            pipe_name[i] = name_start[i];
        }
        pipe_name[name_len] = 0;
        
        uint32_t pipe_id = PipeAPI::CreatePipe(pipe_name);
        if (pipe_id) {
            tty.Write("Created pipe: ");
            tty.Write(pipe_name);
            tty.Write(" (ID: ");
            tty.WriteHex(pipe_id);
            tty.Write(")\n");
        } else {
            tty.Write("Failed to create pipe\n");
        }
        return;
    }

    // Built-in: rmpipe command (remove pipe)
    if (String::strcmp(prog, (const int8_t*)"rmpipe", 6) == 0 &&
        (prog[6] == ' ' || prog[6] == 0)) {
        if (prog[6] == 0) {
            tty.Write("Usage: rmpipe <name>\n");
            return;
        }
        
        // Find pipe name
        const int8_t* name_start = prog + 7;
        while (*name_start == ' ') name_start++; // Skip spaces
        
        if (*name_start == 0) {
            tty.Write("Usage: rmpipe <name>\n");
            return;
        }
        
        // Find end of name
        const int8_t* name_end = name_start;
        while (*name_end && *name_end != ' ') name_end++;
        
        // Extract name
        char pipe_name[32];
        int name_len = name_end - name_start;
        if (name_len >= sizeof(pipe_name)) name_len = sizeof(pipe_name) - 1;
        for (int i = 0; i < name_len; i++) {
            pipe_name[i] = name_start[i];
        }
        pipe_name[name_len] = 0;
        
        if (PipeAPI::DestroyPipe(pipe_name)) {
            tty.Write("Removed pipe: ");
            tty.Write(pipe_name);
            tty.Write("\n");
        } else {
            tty.Write("Failed to remove pipe or pipe not found\n");
        }
        return;
    }

    // Built-in: help command
    if (String::strcmp(prog, (const int8_t*)"help", 4) == 0 &&
        (prog[4] == 0)) {
        tty.Write("KOS Shell Commands:\n");
        tty.Write("  help           - Show this help message\n");
    tty.Write("  clear          - Clear the text screen\n");
    tty.Write("  cls            - Alias for clear\n");
        tty.Write("  logo           - Show KOS logo in text mode\n");
        tty.Write("  logo32         - Show KOS logo on framebuffer\n");
    tty.Write("  gfxinfo        - Show framebuffer info (if available)\n");
    tty.Write("  gfxinit        - Initialize graphics: clear and draw logo\n");
    tty.Write("  gfxclear [hex] - Clear framebuffer to RRGGBB (default black)\n");
    tty.Write("  cursor [style] - Show or set cursor style (crosshair|triangle)\n");
    tty.Write("  mousedbg on/off- Dump raw mouse bytes (IRQ/POLL)\n");
    tty.Write("  cursor [style] - Show or set cursor style (crosshair|triangle)\n");
        tty.Write("  lshw           - Hardware info: CPU, memory, PCI\n");
    tty.Write("  reboot         - Reboot (root only)\n");
    tty.Write("  shutdown       - Power off (root only)\n");
        
        
        tty.Write("  ps             - Show scheduler statistics\n");
        tty.Write("  threads        - Show detailed thread list\n");
        tty.Write("  yield          - Yield CPU to other tasks\n");
        tty.Write("  kill <id>      - Kill thread by ID (hex)\n");
        tty.Write("  suspend <id>   - Suspend thread by ID (hex)\n");
        tty.Write("  resume <id>    - Resume thread by ID (hex)\n");
        tty.Write("  sleep <ms>     - Sleep current thread (decimal ms)\n");
        tty.Write("  pipes          - Show all active pipes\n");
        tty.Write("  mkpipe <name>  - Create a new pipe\n");
        tty.Write("  rmpipe <name>  - Remove a pipe\n");
        tty.Write("  <cmd>          - Execute /bin/<cmd>.elf from filesystem\n");
        tty.Write("  whoami         - Print current user\n");
        tty.Write("  su <u> <p>     - Switch user (login)\n");
        tty.Write("  adduser <u> <p>- Add user (root)\n");
        tty.Write("  passwd <u> <p> - Change password (root or self)\n");
        return;
    }

    // Build /bin/<prog>
    int8_t path[64];
    int32_t cmdLen = String::strlen(prog);
    // "/bin/" (5 chars) + command + null terminator must fit
    if (cmdLen + 5 + 1 > (int32_t)sizeof(path)) {
        tty.Write("Command too long\n");
        return;
    }
    // Build path: "/bin/" + command
    path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
    // Safe copy of program name into path buffer
    String::memmove(path + 5, prog, (uint32_t)cmdLen);
    path[5 + cmdLen] = 0;
    // Try to execute /bin/<cmd>.elf as ELF32
    int8_t elfPath[80];
    int32_t baseLen = String::strlen(path);
    if (baseLen + 4 < (int32_t)sizeof(elfPath)) {
        // Copy base path into elfPath
        String::memmove(elfPath, path, (uint32_t)baseLen);
        elfPath[baseLen] = '.';
        elfPath[baseLen+1] = 'e';
            elfPath[baseLen+2] = 'l'; 
            elfPath[baseLen+3] = 'f'; 
            elfPath[baseLen+4] = 0;
            if (kos::fs::g_fs_ptr) {
                static uint8_t elfBuf[256*1024]; // 256 KB buffer for apps
                int32_t n = kos::fs::g_fs_ptr->ReadFile(elfPath, elfBuf, sizeof(elfBuf));
                if (n > 0) {
                    tty.Write((int8_t*)"Loading ELF ");
                    tty.Write(elfPath);
                    tty.Write((int8_t*)" size=");
                    tty.WriteHex((n >> 8) & 0xFF);
                    tty.WriteHex(n & 0xFF);
                    tty.Write((int8_t*)"...\n");
                    // Set args into system API for the app to read
                    SetArgs(argc, argv, command);
                    if (!ELFLoader::LoadAndExecute(elfBuf, (uint32_t)n)) {
                        tty.Write("ELF load failed\n");
                    }
                    return;
                } else {
                    tty.Write((int8_t*)"ReadFile failed or empty\n");
                }
            }
        }
    tty.Write("Command not found: ");
    tty.Write(prog);
    tty.PutChar('\n');
    
       
}

