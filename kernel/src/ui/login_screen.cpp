#include <ui/login_screen.hpp>
#include <ui/framework.hpp>
#include <services/user_service.hpp>
#include <graphics/framebuffer.hpp>
#include <lib/string.hpp>
#include <console/tty.hpp>
#include <lib/serial.hpp>

using namespace kos::ui;
using namespace kos::gfx;
using namespace kos::services;

uint32_t LoginScreen::s_win_id = 0;
bool     LoginScreen::s_ready = false;
bool     LoginScreen::s_authenticated = false;
char     LoginScreen::s_user[32] = {0};
int      LoginScreen::s_user_len = 0;
char     LoginScreen::s_pass[32] = {0};
int      LoginScreen::s_pass_len = 0;
bool     LoginScreen::s_focus_user = true;
char     LoginScreen::s_message[96] = {0};

void LoginScreen::Initialize(uint32_t windowId) {
    s_win_id = windowId;
    s_ready = (s_win_id != 0);
    s_authenticated = false;
    s_user_len = 0; s_pass_len = 0; s_focus_user = true;
    s_message[0] = 0;
}

static inline void clampAppend(char* buf, int& len, int max, char c) {
    if (len < max - 1) { buf[len++] = c; buf[len] = 0; }
}

void LoginScreen::OnKeyDown(int8_t c) {
    // Debug: log first few key events
    static int key_count = 0;
    if (key_count < 5) {
        kos::lib::serial_write("[LOGIN] OnKeyDown: ");
        if (c >= 32 && c <= 126) {
            kos::lib::serial_putc((char)c);
        } else {
            kos::lib::serial_write("0x");
            const char* hex = "0123456789ABCDEF";
            kos::lib::serial_putc(hex[((uint8_t)c >> 4) & 0xF]);
            kos::lib::serial_putc(hex[(uint8_t)c & 0xF]);
        }
        kos::lib::serial_write("\n");
        ++key_count;
    }
    
    if (!s_ready || s_authenticated) return;
    if (c == '\n') {
        // Try auth via UserService
        UserService* us = GetUserService();
        if (us && us->Authenticate((const int8_t*)s_user, (const int8_t*)s_pass)) {
            // Login and mark authenticated
            if (us->Login((const int8_t*)s_user, (const int8_t*)s_pass)) {
                s_authenticated = true;
                // Clear sensitive pass buffer
                for (int i=0;i<s_pass_len;++i) s_pass[i] = 0;
                s_pass_len = 0;
                // message
                const char ok[] = "Login successful";
                int mi=0; for (int i=0; ok[i] && mi<95; ++i) s_message[mi++]=ok[i]; s_message[mi]=0;
            } else {
                const char msg[] = "Login failed";
                int mi=0; for (int i=0; msg[i] && mi<95; ++i) s_message[mi++]=msg[i]; s_message[mi]=0;
            }
        } else {
            const char msg[] = "Invalid credentials";
            int mi=0; for (int i=0; msg[i] && mi<95; ++i) s_message[mi++]=msg[i]; s_message[mi]=0;
        }
    } else if (c == '\b') {
        if (s_focus_user) { if (s_user_len > 0) { s_user[--s_user_len] = 0; } }
        else { if (s_pass_len > 0) { s_pass[--s_pass_len] = 0; } }
    } else if (c == 0x09) { // Tab
        s_focus_user = !s_focus_user;
    } else {
        // Only accept printable ASCII
        if (c >= 32 && c <= 126) {
            if (s_focus_user) clampAppend(s_user, s_user_len, 32, (char)c);
            else clampAppend(s_pass, s_pass_len, 32, (char)c);
            // Typing hint to confirm key reception
            const char typing[] = "Typing...";
            int mi=0; for (int i=0; typing[i] && mi<95; ++i) s_message[mi++]=typing[i]; s_message[mi]=0;
        }
    }
}

bool LoginScreen::Authenticated() { return s_authenticated; }
const char* LoginScreen::Username() { return s_user; }

void LoginScreen::drawText(uint32_t x, uint32_t y, const char* text, uint32_t fg, uint32_t bg) {
    for (uint32_t i=0; text[i]; ++i) {
        char ch = text[i]; if (ch < 32 || ch > 127) ch='?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        Compositor::DrawGlyph8x8(x + i*8, y, glyph, fg, bg);
    }
}

void LoginScreen::Render() {
    if (!s_ready) return;
    WindowDesc d; if (!kos::ui::GetWindowDesc(s_win_id, d)) return;
    const uint32_t th = kos::ui::TitleBarHeight();
    const uint32_t padX = 12; const uint32_t padY = 12;
    uint32_t x0 = d.x + padX; uint32_t y0 = d.y + th + padY;
    // Clear client area
    Compositor::FillRect(d.x, d.y + th, d.w, d.h > th ? d.h - th : 0, d.bg);
    // Title
    drawText(x0, y0, "Welcome to KOS", 0xFFFFFFFFu, d.bg);
    // Username label and field
    drawText(x0, y0 + 16, "Username:", 0xFFB0B0B0u, d.bg);
    // Draw a simple box background for input
    uint32_t ux = x0 + 80; uint32_t uy = y0 + 16; uint32_t uw = d.w - (ux - d.x) - padX; if (uw < 80) uw = 80;
    Compositor::FillRect(ux, uy-2, uw, 12, 0xFF222225u);
    drawText(ux + 4, uy, s_user, 0xFFFFFFFFu, 0xFF222225u);
    // Password
    drawText(x0, y0 + 32, "Password:", 0xFFB0B0B0u, d.bg);
    uint32_t px = x0 + 80; uint32_t py = y0 + 32; uint32_t pw = d.w - (px - d.x) - padX; if (pw < 80) pw = 80;
    Compositor::FillRect(px, py-2, pw, 12, 0xFF222225u);
    // Masked pass
    char masked[32]; for (int i=0;i<s_pass_len && i<31;++i) masked[i]='*'; masked[s_pass_len<31?s_pass_len:31]=0;
    drawText(px + 4, py, masked, 0xFFFFFFFFu, 0xFF222225u);
    // Focus indicator
    uint32_t fx = s_focus_user ? ux : px; uint32_t fy = s_focus_user ? uy : py; uint32_t fw = s_focus_user ? uw : pw;
    Compositor::FillRect(fx, fy+10, fw, 1, 0xFF3B82F6u);
    // Hint or status
    const char* hint = "Press Enter to login, Tab to switch";
    drawText(x0, y0 + 52, hint, 0xFF909090u, d.bg);
    if (s_message[0]) drawText(x0, y0 + 66, s_message, 0xFFFF6B6Bu, d.bg);
}
