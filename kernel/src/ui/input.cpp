#include <ui/input.hpp>
#include <graphics/framebuffer.hpp>

using namespace kos::ui;

namespace kos { namespace ui {

static int g_mx = 10; // default cursor position
static int g_my = 10;
static uint8_t g_mb = 0; // buttons bitset
static int g_sens_num = 1; // sensitivity numerator
static int g_sens_den = 1; // sensitivity denominator

void InitInput() {
    g_mx = 10; g_my = 10; g_mb = 0; g_sens_num = 1; g_sens_den = 1;
}

void MouseMove(int dx, int dy) {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    // apply sensitivity scaling
    if (g_sens_den <= 0) g_sens_den = 1;
    dx = (dx * g_sens_num) / g_sens_den;
    dy = (dy * g_sens_num) / g_sens_den;
    g_mx += dx; g_my += dy;
    if (g_mx < 0) g_mx = 0; if (g_my < 0) g_my = 0;
    if (g_mx >= (int)fb.width) g_mx = (int)fb.width - 1;
    if (g_my >= (int)fb.height) g_my = (int)fb.height - 1;
}

void MouseButtonDown(uint8_t button) {
    if (button < 1 || button > 3) return;
    g_mb |= (1u << (button - 1));
}

void MouseButtonUp(uint8_t button) {
    if (button < 1 || button > 3) return;
    g_mb &= ~(1u << (button - 1));
}

void GetMouseState(int& x, int& y, uint8_t& buttons) {
    x = g_mx; y = g_my; buttons = g_mb;
}

void SetCursorPos(int x, int y) {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x >= (int)fb.width) x = (int)fb.width - 1;
    if (y >= (int)fb.height) y = (int)fb.height - 1;
    g_mx = x; g_my = y;
}

void SetMouseSensitivity(int num, int den) {
    if (num <= 0) num = 1; if (den <= 0) den = 1;
    g_sens_num = num; g_sens_den = den;
}

}} // namespace
