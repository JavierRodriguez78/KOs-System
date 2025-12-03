#ifndef KOS_UI_LOGIN_SCREEN_HPP
#define KOS_UI_LOGIN_SCREEN_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>
#include <graphics/font8x8_basic.hpp>

namespace kos { namespace ui {

class LoginScreen {
public:
    // Initialize with a window id to render into
    static void Initialize(uint32_t windowId);
    // Render the login UI contents into the window
    static void Render();
    // Process a key press (ASCII, special: '\n' enter, '\b' backspace, 0x09 tab)
    static void OnKeyDown(int8_t c);
    // Returns true once authentication succeeded
    static bool Authenticated();
    // Get entered username (for post-login greeting)
    static const char* Username();
private:
    static uint32_t s_win_id;
    static bool s_ready;
    static bool s_authenticated;
    static char s_user[32];
    static int  s_user_len;
    static char s_pass[32];
    static int  s_pass_len;
    static bool s_focus_user; // true=user field, false=pass field
    static char s_message[96]; // status / error text
    static void drawText(uint32_t x, uint32_t y, const char* text, uint32_t fg, uint32_t bg);
};

}}

#endif // KOS_UI_LOGIN_SCREEN_HPP
