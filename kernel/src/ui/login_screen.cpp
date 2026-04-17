#include <ui/login_screen.hpp>
#include <ui/framework.hpp>
#include <services/user_service.hpp>
#include <graphics/framebuffer.hpp>
#include <lib/stdio.hpp>
#include <lib/string.hpp>
#include <console/tty.hpp>

using namespace kos::ui;
using namespace kos::gfx;
using namespace kos::services;

uint32_t LoginScreen::s_win_id = 0;
bool     LoginScreen::s_ready = false;
bool     LoginScreen::s_authenticated = false;
char     LoginScreen::s_users[LoginScreen::kMaxListedUsers][32] = {{0}};
int      LoginScreen::s_user_count = 0;
int      LoginScreen::s_selected_user = -1;
char     LoginScreen::s_user[32] = {0};
int      LoginScreen::s_user_len = 0;
char     LoginScreen::s_pass[32] = {0};
int      LoginScreen::s_pass_len = 0;
bool     LoginScreen::s_focus_user = true;
uint8_t  LoginScreen::s_session_idx = 0;
char     LoginScreen::s_message[96] = {0};
LoginAction LoginScreen::s_pending_action = LoginAction::None;
bool     LoginScreen::s_user_menu_open = false;

void LoginScreen::Initialize(uint32_t windowId) {
    s_win_id = windowId;
    s_ready = (s_win_id != 0);
    s_authenticated = false;
    s_user_len = 0; s_pass_len = 0; s_focus_user = true;
    s_session_idx = 0;
    s_message[0] = 0;
    s_pending_action = LoginAction::None;
    s_user_menu_open = false;
    loadUsersFromService();
}

static inline void clampAppend(char* buf, int& len, int max, char c) {
    if (len < max - 1) { buf[len++] = c; buf[len] = 0; }
}

static void appendDec2(char* out, int& pos, int value) {
    out[pos++] = char('0' + ((value / 10) % 10));
    out[pos++] = char('0' + (value % 10));
}

static void appendDec4(char* out, int& pos, int value) {
    out[pos++] = char('0' + ((value / 1000) % 10));
    out[pos++] = char('0' + ((value / 100) % 10));
    out[pos++] = char('0' + ((value / 10) % 10));
    out[pos++] = char('0' + (value % 10));
}

void LoginScreen::selectUser(int idx) {
    if (idx < 0 || idx >= s_user_count) return;
    s_selected_user = idx;
    s_user_len = 0;
    while (s_users[idx][s_user_len] && s_user_len < 31) {
        s_user[s_user_len] = s_users[idx][s_user_len];
        ++s_user_len;
    }
    s_user[s_user_len] = 0;
}

void LoginScreen::loadUsersFromService() {
    s_user_count = 0;
    s_selected_user = -1;
    for (int i = 0; i < kMaxListedUsers; ++i) s_users[i][0] = 0;

    UserService* us = GetUserService();
    if (!us) return;
    int32_t n = us->UserCount();
    if (n < 0) n = 0;
    if (n > kMaxListedUsers) n = kMaxListedUsers;

    for (int32_t i = 0; i < n; ++i) {
        const User* u = us->UserAt(i);
        if (!u || u->locked) continue;
        int out = s_user_count;
        int j = 0;
        while (u->name[j] && j < 31) {
            s_users[out][j] = (char)u->name[j];
            ++j;
        }
        s_users[out][j] = 0;
        ++s_user_count;
    }

    if (s_user_count > 0) {
        selectUser(0);
    }
}

void LoginScreen::OnKeyDown(int8_t c) {
    if (!s_ready || s_authenticated) return;
    if (c == '\n') {
        (void)tryAuthenticate();
    } else if (c == '\b') {
        if (s_focus_user) { if (s_user_len > 0) { s_user[--s_user_len] = 0; } }
        else { if (s_pass_len > 0) { s_pass[--s_pass_len] = 0; } }
    } else if (c == 0x09) { // Tab
        s_focus_user = !s_focus_user;
    } else if (c == ' ') {
        s_session_idx = (uint8_t)((s_session_idx + 1u) % 2u);
    } else {
        // Only accept printable ASCII
        if (c >= 32 && c <= 126) {
            if (s_focus_user) clampAppend(s_user, s_user_len, 32, (char)c);
            else clampAppend(s_pass, s_pass_len, 32, (char)c);
            // Clear any previous error message when user starts typing
            s_message[0] = 0;
        }
    }
}

bool LoginScreen::tryAuthenticate() {
    UserService* us = GetUserService();
    if (us && us->Authenticate((const int8_t*)s_user, (const int8_t*)s_pass)) {
        if (us->Login((const int8_t*)s_user, (const int8_t*)s_pass)) {
            s_authenticated = true;
            for (int i=0;i<s_pass_len;++i) s_pass[i] = 0;
            s_pass_len = 0;
            const char ok[] = "Login successful";
            int mi=0; for (int i=0; ok[i] && mi<95; ++i) s_message[mi++]=ok[i]; s_message[mi]=0;
            return true;
        }
        const char msg[] = "Login failed";
        int mi=0; for (int i=0; msg[i] && mi<95; ++i) s_message[mi++]=msg[i]; s_message[mi]=0;
        return false;
    }
    const char msg[] = "Invalid credentials";
    int mi=0; for (int i=0; msg[i] && mi<95; ++i) s_message[mi++]=msg[i]; s_message[mi]=0;
    return false;
}

bool LoginScreen::Authenticated() { return s_authenticated; }
const char* LoginScreen::Username() { return s_user; }
LoginAction LoginScreen::ConsumePendingAction() {
    LoginAction a = s_pending_action;
    s_pending_action = LoginAction::None;
    return a;
}

static uint32_t textWidth(const char* text) {
    uint32_t width = 0;
    while (text && text[width]) ++width;
    return width * 8u;
}

static void drawCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell = 2) {
    if (cell == 0) cell = 1;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            uint32_t tileX = (i / cell);
            uint32_t tileY = (j / cell);
            uint32_t color = ((tileX + tileY) & 1u) ? cA : cB;
            Compositor::FillRect(x + i, y + j, 1, 1, color);
        }
    }
}

void LoginScreen::OnPointerDown(int x, int y) {
    if (!s_ready || s_authenticated) return;
    WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_win_id, d)) return;
    const uint32_t th = 0;
    const uint32_t padX = 18;
    const uint32_t padY = 16;
    uint32_t x0 = d.x + padX;
    uint32_t y0 = d.y + th + padY;

    uint32_t centerX = d.x + d.w / 2u;
    uint32_t selectorW = 164;
    uint32_t selectorX = centerX - selectorW / 2u;
    uint32_t selectorY = y0 + 74;
    uint32_t selectorH = 16;
    uint32_t listW = selectorW;
    uint32_t listX = selectorX;
    uint32_t listY = selectorY + selectorH + 6;
    uint32_t listH = (s_user_count > 0) ? (uint32_t)s_user_count * 12u + 8u : 18u;
    bool selectedUser = s_selected_user >= 0 && s_selected_user < s_user_count;

    uint32_t fieldW = d.w > padX * 2u ? d.w - padX * 2u : d.w;
    if (fieldW > 220) fieldW = 220;
    uint32_t fieldX = centerX - fieldW / 2u;
    uint32_t ux = fieldX; uint32_t uy = y0 + 118; uint32_t uw = fieldW;
    uint32_t px = fieldX; uint32_t py = selectedUser ? (y0 + 176) : (y0 + 202); uint32_t pw = fieldW;

    auto inRect=[&](uint32_t rx,uint32_t ry,uint32_t rw,uint32_t rh){
        return (uint32_t)x >= rx && (uint32_t)x < rx + rw && (uint32_t)y >= ry && (uint32_t)y < ry + rh;
    };

    if (inRect(selectorX, selectorY, selectorW, selectorH)) {
        s_user_menu_open = !s_user_menu_open;
        return;
    }

    if (s_user_menu_open) {
        for (int i = 0; i < s_user_count; ++i) {
            uint32_t ry = listY + 4 + (uint32_t)i * 12;
            if (inRect(listX + 4, ry, listW - 8, 10)) {
                selectUser(i);
                s_user_menu_open = false;
                s_focus_user = false;
                s_message[0] = 0;
                return;
            }
        }
        if (!inRect(listX, listY, listW, listH)) s_user_menu_open = false;
    }

    if (!selectedUser && inRect(ux, uy, uw, 16)) { s_focus_user = true; return; }
    if (inRect(px, py, pw, 16)) { s_focus_user = false; return; }

    // Session selector button
    uint32_t sx = centerX - 82u; uint32_t sy = selectedUser ? (y0 + 218) : (y0 + 246); uint32_t sw = 164; uint32_t sh = 16;
    if (inRect(sx, sy, sw, sh)) { s_session_idx = (uint8_t)((s_session_idx + 1u) % 2u); return; }

    // Sign in button
    uint32_t bx = centerX - 42u; uint32_t by = selectedUser ? (y0 + 240) : (y0 + 274); uint32_t bw = 84; uint32_t bh = 20;
    if (inRect(bx, by, bw, bh)) { (void)tryAuthenticate(); return; }

    // Power buttons
    uint32_t rbX = centerX - 24u; uint32_t rbY = selectedUser ? (y0 + 266) : (y0 + 306); uint32_t rbW = 18; uint32_t rbH = 14;
    uint32_t sbX = centerX + 6u; uint32_t sbY = selectedUser ? (y0 + 266) : (y0 + 306); uint32_t sbW = 18; uint32_t sbH = 14;
    if (inRect(rbX, rbY, rbW, rbH)) { s_pending_action = LoginAction::Reboot; return; }
    if (inRect(sbX, sbY, sbW, sbH)) { s_pending_action = LoginAction::Shutdown; return; }
}

void LoginScreen::drawText(uint32_t x, uint32_t y, const char* text, uint32_t fg, uint32_t bg) {
    const uint32_t shadow = 0xFF101010u;
    for (uint32_t i=0; text[i]; ++i) {
        char ch = text[i]; if (ch < 32 || ch > 127) ch='?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
        Compositor::DrawGlyph8x8(x + i*8 + 1, y + 1, glyph, shadow, bg);
        Compositor::DrawGlyph8x8(x + i*8, y, glyph, fg, bg);
    }
}

void LoginScreen::Render() {
    if (!s_ready) return;
    WindowDesc d; 
    if (!kos::ui::GetWindowDesc(s_win_id, d)) return;
    const uint32_t th = 0;
    const uint32_t padX = 18; const uint32_t padY = 16;
    uint32_t x0 = d.x + padX; uint32_t y0 = d.y + th + padY;
    uint32_t centerX = d.x + d.w / 2u;
    bool selectedUser = s_selected_user >= 0 && s_selected_user < s_user_count;
    // Clear entire client area first
    drawCheckerRect(d.x, d.y + th, d.w, d.h > th ? d.h - th : 0, 0xFF080A12u, 0xFF0A0D16u, 2);
    Compositor::FillRect(d.x, d.y, d.w, 1, 0x66FFFFFFu);
    Compositor::FillRect(d.x, d.y + d.h - 1, d.w, 1, 0x66000000u);
    Compositor::FillRect(d.x, d.y, 1, d.h, 0x66FFFFFFu);
    Compositor::FillRect(d.x + d.w - 1, d.y, 1, d.h, 0x66000000u);
    // Title
    const char* title = "Welcome to KOS";
    drawText(centerX - textWidth(title) / 2u, y0, title, 0xFFFFFFFFu, d.bg);
    const char* subtitle = "Sign in to continue";
    drawText(centerX - textWidth(subtitle) / 2u, y0 + 12, subtitle, 0xFFB7BCC7u, d.bg);

    // Header date/time from RTC
    {
        uint16_t year = 0; uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
        kos::sys::get_datetime(&year, &month, &day, &hour, &minute, &second);
        char dt[32]; int p = 0;
        if (year != 0) {
            appendDec4(dt, p, (int)year); dt[p++] = '-';
            appendDec2(dt, p, (int)month); dt[p++] = '-';
            appendDec2(dt, p, (int)day); dt[p++] = ' ';
            appendDec2(dt, p, (int)hour); dt[p++] = ':';
            appendDec2(dt, p, (int)minute); dt[p++] = ':';
            appendDec2(dt, p, (int)second);
            dt[p] = 0;
            uint32_t dtX = centerX - (uint32_t)p * 4u;
            drawText(dtX, y0 + 28, dt, 0xFF8F96A5u, d.bg);
        }
    }

    // Centered session card
    uint32_t sessionCardX = centerX - 108u;
    uint32_t sessionCardY = y0 + 44;
    uint32_t sessionCardW = 216u;
    uint32_t sessionCardH = selectedUser ? 104u : 112u;
    drawCheckerRect(sessionCardX, sessionCardY, sessionCardW, sessionCardH, 0xFF15171Cu, 0xFF171A20u, 2);
    Compositor::FillRect(sessionCardX, sessionCardY, sessionCardW, 1, 0x22FFFFFFu);
    Compositor::FillRect(sessionCardX, sessionCardY + sessionCardH - 1, sessionCardW, 1, 0x44000000u);
    Compositor::FillRect(sessionCardX, sessionCardY, 1, sessionCardH, 0x22FFFFFFu);
    Compositor::FillRect(sessionCardX + sessionCardW - 1, sessionCardY, 1, sessionCardH, 0x44000000u);

    uint32_t avatarX = centerX - 24u;
    uint32_t avatarY = sessionCardY + 14u;
    uint32_t avatarBg = 0xFF334155u;
    if (s_selected_user == 0) avatarBg = 0xFF2563EBu;
    else if (s_selected_user == 1) avatarBg = 0xFF7C3AEDu;
    else if (s_selected_user > 1) avatarBg = 0xFF0F766Eu;
    Compositor::FillRect(avatarX - 2, avatarY - 2, 52, 52, 0xFF1D2028u);
    Compositor::FillRect(avatarX, avatarY, 48, 48, avatarBg);
    char initials[3] = {'?', 0, 0};
    if (selectedUser && s_users[s_selected_user][0]) {
        initials[0] = s_users[s_selected_user][0];
        int n = 1;
        while (s_users[s_selected_user][n] == ' ') ++n;
        if (s_users[s_selected_user][n]) initials[1] = s_users[s_selected_user][n];
    }
    drawText(avatarX + 16, avatarY + 18, initials, 0xFFFFFFFFu, avatarBg);

    const char* currentUser = selectedUser ? s_users[s_selected_user] : "guest";
    uint32_t nameY = avatarY + 62;
    drawText(centerX - textWidth(currentUser) / 2u, nameY, currentUser, 0xFFE5E7EBu, d.bg);

    if (selectedUser) {
        const char* prompt = "Select another user";
        drawText(centerX - textWidth(prompt) / 2u, nameY + 12, prompt, 0xFF8F96A5u, d.bg);
    } else {
        const char* prompt = "Choose a user to sign in";
        drawText(centerX - textWidth(prompt) / 2u, nameY + 12, prompt, 0xFF8F96A5u, d.bg);
    }

    uint32_t selectorW = 164;
    uint32_t selectorX = centerX - selectorW / 2u;
    uint32_t selectorY = sessionCardY + sessionCardH - 28u;
    uint32_t selectorH = 16;
    Compositor::FillRect(selectorX, selectorY, selectorW, selectorH, 0xFF1B1E25u);
    Compositor::FillRect(selectorX, selectorY, selectorW, 1, 0x1AFFFFFFu);
    drawText(selectorX + 8, selectorY + 4, currentUser, 0xFFE8ECF2u, 0xFF1B1E25u);
    drawText(selectorX + selectorW - 18, selectorY + 4, s_user_menu_open ? "^" : "v", 0xFFA3ABBAu, 0xFF1B1E25u);

    uint32_t listX = selectorX;
    uint32_t listY = selectorY + selectorH + 6;
    uint32_t listW = selectorW;
    uint32_t listH = (s_user_count > 0) ? (uint32_t)s_user_count * 12u + 8u : 18u;
    if (s_user_menu_open) {
        drawCheckerRect(listX, listY, listW, listH, 0xFF191C23u, 0xFF1B1E26u, 2);
        if (s_user_count == 0) {
            drawText(listX + 8, listY + 5, "(no users)", 0xFF909090u, 0xFF191A1Fu);
        } else {
            for (int i = 0; i < s_user_count; ++i) {
                uint32_t ry = listY + 4 + (uint32_t)i * 12;
                uint32_t rowBg = (i == s_selected_user) ? 0xFF2C3342u : 0xFF191C23u;
                uint32_t rowFg = (i == s_selected_user) ? 0xFFF1F5FBu : 0xFFD5DBE7u;
                Compositor::FillRect(listX + 4, ry, listW - 8, 10, rowBg);
                drawText(listX + 10, ry + 2, s_users[i], rowFg, rowBg);
            }
        }
    }

    uint32_t fieldW = d.w > padX * 2u ? d.w - padX * 2u : d.w;
    if (fieldW > 220) fieldW = 220;
    uint32_t formX = centerX - fieldW / 2u;
    uint32_t passwordLabelY = selectedUser ? (y0 + 164) : (y0 + 200);
    uint32_t passwordFieldY = selectedUser ? (y0 + 176) : (y0 + 212);
    
    uint32_t ux = formX; uint32_t uy = y0 + 170; uint32_t uw = fieldW;
    if (!selectedUser) {
        drawText(formX, y0 + 158, "Username", 0xFFB0B0B0u, d.bg);
        Compositor::FillRect(ux, uy, uw, 16, 0xFF222225u);
        drawText(ux + 4, uy + 3, s_user, 0xFFFFFFFFu, 0xFF222225u);
        if (s_focus_user) {
            uint32_t cursorX = ux + 4 + (uint32_t)s_user_len * 8;
            Compositor::FillRect(cursorX, uy + 3, 2, 10, 0xFFFFFFFFu);
        }
    }
    
    // Password label and field
    drawText(formX, passwordLabelY, selectedUser ? "Password for selected user" : "Password", 0xFFB0B0B0u, d.bg);
    uint32_t px = formX; uint32_t py = passwordFieldY; uint32_t pw = fieldW;
    Compositor::FillRect(px, py, pw, 16, 0xFF222225u);
    // Masked password
    char masked[32]; for (int i=0;i<s_pass_len && i<31;++i) masked[i]='*'; masked[s_pass_len<31?s_pass_len:31]=0;
    drawText(px + 4, py + 3, masked, 0xFFFFFFFFu, 0xFF222225u);
    // Draw cursor if focused on password field
    if (!s_focus_user) {
        uint32_t cursorX = px + 4 + (uint32_t)s_pass_len * 8;
        Compositor::FillRect(cursorX, py + 3, 2, 10, 0xFFFFFFFFu);
    }
    
    // Focus indicator line under the active field
    bool focusUserField = (!selectedUser && s_focus_user);
    uint32_t fx = focusUserField ? ux : px; uint32_t fy = focusUserField ? uy : py; uint32_t fw = focusUserField ? uw : pw;
    Compositor::FillRect(fx, fy + 16, fw, 2, 0xFF3B82F6u);
    
    const char* hint1 = selectedUser ? "Enter=Sign In   Click user to switch" : "Enter=Sign In   Tab=Switch Field";
    drawText(centerX - textWidth(hint1) / 2u, selectedUser ? (y0 + 200) : (y0 + 240), hint1, 0xFF909090u, d.bg);

    // Session selector (Ubuntu-like option)
    uint32_t sx = centerX - 82u; uint32_t sy = selectedUser ? (y0 + 218) : (y0 + 256); uint32_t sw = 164; uint32_t sh = 16;
    Compositor::FillRect(sx, sy, sw, sh, 0xFF22252Cu);
    const char* sess = (s_session_idx == 0) ? "Session: KOS Desktop" : "Session: Recovery";
    drawText(sx + 4, sy + 4, sess, 0xFFE9EDF4u, 0xFF22252Cu);

    // Sign in button
    uint32_t bx = centerX - 42u; uint32_t by = selectedUser ? (y0 + 240) : (y0 + 284); uint32_t bw = 84; uint32_t bh = 20;
    Compositor::FillRect(bx, by, bw, bh, 0xFF3B82F6u);
    drawText(bx + 10, by + 6, "Sign In", 0xFFFFFFFFu, 0xFF3B82F6u);

    // Power options
    uint32_t rbX = centerX - 24u; uint32_t rbY = selectedUser ? (y0 + 266) : (y0 + 306); uint32_t rbW = 18; uint32_t rbH = 14;
    uint32_t sbX = centerX + 6u; uint32_t sbY = selectedUser ? (y0 + 266) : (y0 + 306); uint32_t sbW = 18; uint32_t sbH = 14;
    uint32_t iconBg = 0xFF252932u;
    uint32_t iconFg = 0xFFC7CFDEu;
    Compositor::FillRect(rbX, rbY, rbW, rbH, iconBg);
    Compositor::FillRect(sbX, sbY, sbW, rbH, iconBg);
    Compositor::FillRect(rbX, rbY, rbW, 1, 0x1AFFFFFFu);
    Compositor::FillRect(sbX, sbY, sbW, 1, 0x1AFFFFFFu);

    // Reboot icon: small loop + arrow tip
    Compositor::FillRect(rbX + 5, rbY + 4, 7, 1, iconFg);
    Compositor::FillRect(rbX + 5, rbY + 8, 7, 1, iconFg);
    Compositor::FillRect(rbX + 4, rbY + 5, 1, 3, iconFg);
    Compositor::FillRect(rbX + 12, rbY + 5, 1, 2, iconFg);
    Compositor::FillRect(rbX + 11, rbY + 3, 2, 1, iconFg);
    Compositor::FillRect(rbX + 12, rbY + 2, 1, 1, iconFg);

    // Power icon: ring + vertical stem
    Compositor::FillRect(sbX + 6, sbY + 4, 5, 1, iconFg);
    Compositor::FillRect(sbX + 6, sbY + 8, 5, 1, iconFg);
    Compositor::FillRect(sbX + 5, sbY + 5, 1, 3, iconFg);
    Compositor::FillRect(sbX + 11, sbY + 5, 1, 3, iconFg);
    Compositor::FillRect(sbX + 8, sbY + 2, 1, 4, iconFg);
    
    // Status/error message
    if (s_message[0]) drawText(centerX - textWidth(s_message) / 2u, selectedUser ? (y0 + 290) : (y0 + 338), s_message, 0xFFFF6B6Bu, d.bg);
    
}
