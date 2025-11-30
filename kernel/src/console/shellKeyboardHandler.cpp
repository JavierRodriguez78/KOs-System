#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <console/tty.hpp>
#ifndef KOS_BUILD_APPS
#include <graphics/terminal.hpp>
#include <ui/framework.hpp>
#endif
#include <drivers/keyboard/keyboard.hpp>
#include <lib/stdio.hpp>
#include <kernel/globals.hpp>
#include <console/logger.hpp>


using namespace kos::common;    
using namespace kos::console;


extern "C" void sys_offer_key(int8_t c);


ShellKeyboardHandler::ShellKeyboardHandler(){
};

ShellKeyboardHandler::~ShellKeyboardHandler(){

};


void ShellKeyboardHandler::OnKeyDown(int8_t c){
    // One-time log to confirm the shell handler is receiving keys
    {
        static bool s_first_key_seen = false;
        if (!s_first_key_seen) { kos::console::Logger::LogKV("KEY", "first-to-shell"); s_first_key_seen = true; }
        if (c == '\n') { kos::console::Logger::LogKV("KEY", "enter"); }
    }
    // Offer key to app-facing queue so applications can poll (non-blocking)
    sys_offer_key(c);
    // Always prioritize routing to shell in graphics mode to avoid input blockage.
    // In text mode, legacy behavior remains unchanged.
    bool consumed = false;
    bool graphics = false;
    #ifndef KOS_BUILD_APPS
    graphics = kos::gfx::IsAvailable();
    #endif
    if (graphics) {
        // Do not let stdio consume keys in graphics mode; terminal window displays TTY output.
        consumed = false;
    } else {
        consumed = kos::sys::TryDeliverKey(c);
    }
    #ifndef KOS_BUILD_APPS
    // Handle scrollback navigation when terminal focused
    if (kos::gfx::Terminal::IsActive()) {
        uint32_t focused = kos::ui::GetFocusedWindow();
        // Accept input if terminal is focused OR focus is unset
        if (focused == 0 || focused == kos::gfx::Terminal::GetWindowId()) {
            if (c == (int8_t)0xF1) { // PageUp
                kos::gfx::Terminal::ScrollPageUp();
                return;
            } else if (c == (int8_t)0xF2) { // PageDown
                kos::gfx::Terminal::ScrollPageDown();
                return;
            }
        }
    }
    #endif
    // Route to shell: prefer terminal when active; if focus is unknown, allow input by default
    bool routeToShell = true;
    #ifndef KOS_BUILD_APPS
    if (kos::gfx::Terminal::IsActive()) {
        // Always route to shell when terminal active (ignore focus entirely for robustness)
        routeToShell = true;
    } else {
        routeToShell = !consumed; // fallback behavior
    }
    #else
    routeToShell = !consumed;
    #endif

    if (routeToShell) {
        // Optional: could add debug echo here if needed
        // Priority: threaded shell if available, otherwise fallback to original shell
        if (kos::console::g_threaded_shell) {
            kos::console::g_threaded_shell->OnKeyPressed(c);
        } else if (kos::g_shell) {
            kos::g_shell->InputChar(c);
        }
    }
}