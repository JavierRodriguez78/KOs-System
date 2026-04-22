#include <ui/input.hpp>
#include <graphics/framebuffer.hpp>
#include <fs/filesystem.hpp>

using namespace kos::ui;

namespace kos { namespace ui {

static int g_mx = 10; // default cursor position
static int g_my = 10;
static uint8_t g_mb = 0; // buttons bitset
static int g_sens_num = 1; // sensitivity numerator
static int g_sens_den = 1; // sensitivity denominator
static int g_space_w = 0; // logical desktop width for mapping
static int g_space_h = 0; // logical desktop height for mapping
static int32_t g_lx_fp = 0; // logical X in Q24.8 fixed-point
static int32_t g_ly_fp = 0; // logical Y in Q24.8 fixed-point
static constexpr int32_t kFpShift = 8;
static constexpr int32_t kFpOne = (1 << kFpShift);

struct DesktopProfile {
    int w;
    int h;
};

static const int8_t* kMouseCfgPathUpper = (const int8_t*)"/ETC/MOUSE.CFG";
static const int8_t* kMouseCfgPathLower = (const int8_t*)"/etc/mouse.cfg";

static inline int32_t Clamp32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int32_t Abs32(int32_t v) {
    return (v < 0) ? -v : v;
}

static bool AspectNear(int w, int h, int num, int den) {
    if (w <= 0 || h <= 0 || num <= 0 || den <= 0) return false;
    int32_t lhs = (int32_t)(w * den);
    int32_t rhs = (int32_t)(h * num);
    int32_t diff = Abs32(lhs - rhs);
    // Around 5% tolerance so common VESA modes still match.
    return diff * 20 <= rhs;
}

static bool ParseMouseCfg(const uint8_t* buf, uint32_t len, int& outW, int& outH) {
    if (!buf || len == 0) return false;

    uint32_t i = 0;
    while (i < len) {
        while (i < len && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) ++i;
        if (i >= len) break;
        // Skip comments
        if (buf[i] == '#') {
            while (i < len && buf[i] != '\n') ++i;
            continue;
        }

        int w = 0;
        bool hasW = false;
        while (i < len && buf[i] >= '0' && buf[i] <= '9') {
            hasW = true;
            w = w * 10 + (int)(buf[i] - '0');
            ++i;
        }
        if (!hasW || i >= len || !(buf[i] == 'x' || buf[i] == 'X')) {
            while (i < len && buf[i] != '\n') ++i;
            continue;
        }
        ++i; // skip x

        int h = 0;
        bool hasH = false;
        while (i < len && buf[i] >= '0' && buf[i] <= '9') {
            hasH = true;
            h = h * 10 + (int)(buf[i] - '0');
            ++i;
        }

        if (hasH && w > 0 && h > 0) {
            outW = w;
            outH = h;
            return true;
        }

        while (i < len && buf[i] != '\n') ++i;
    }

    return false;
}

static void PickProfileAtLeast(const DesktopProfile* profiles, int count, int fbw, int fbh, int& outW, int& outH) {
    outW = fbw;
    outH = fbh;
    for (int i = 0; i < count; ++i) {
        if (profiles[i].w >= fbw && profiles[i].h >= fbh) {
            outW = profiles[i].w;
            outH = profiles[i].h;
            return;
        }
    }
}

static void EnsureDesktopSpace() {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;
    if (g_space_w <= 0) g_space_w = (int)fb.width;
    if (g_space_h <= 0) g_space_h = (int)fb.height;
}

static void ProjectLogicalToScreen() {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;
    EnsureDesktopSpace();

    int32_t max_space_x = (g_space_w > 1) ? (g_space_w - 1) : 0;
    int32_t max_space_y = (g_space_h > 1) ? (g_space_h - 1) : 0;
    int32_t max_space_x_fp = (max_space_x << kFpShift);
    int32_t max_space_y_fp = (max_space_y << kFpShift);
    if (g_lx_fp < 0) g_lx_fp = 0;
    if (g_ly_fp < 0) g_ly_fp = 0;
    if (g_lx_fp > max_space_x_fp) g_lx_fp = max_space_x_fp;
    if (g_ly_fp > max_space_y_fp) g_ly_fp = max_space_y_fp;

    int32_t logical_x = (g_lx_fp >> kFpShift);
    int32_t logical_y = (g_ly_fp >> kFpShift);
    if (g_space_w > 1 && fb.width > 1) {
        g_mx = (int)(((uint32_t)logical_x * (uint32_t)(fb.width - 1)) / (uint32_t)(g_space_w - 1));
    } else {
        g_mx = 0;
    }
    if (g_space_h > 1 && fb.height > 1) {
        g_my = (int)(((uint32_t)logical_y * (uint32_t)(fb.height - 1)) / (uint32_t)(g_space_h - 1));
    } else {
        g_my = 0;
    }

    g_mx = Clamp32(g_mx, 0, (int32_t)fb.width - 1);
    g_my = Clamp32(g_my, 0, (int32_t)fb.height - 1);
}

static void ProjectScreenToLogical(int x, int y) {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;
    EnsureDesktopSpace();

    x = Clamp32(x, 0, (int32_t)fb.width - 1);
    y = Clamp32(y, 0, (int32_t)fb.height - 1);

    int32_t lx = 0;
    int32_t ly = 0;
    if (fb.width > 1 && g_space_w > 1) {
        lx = (int32_t)(((uint32_t)x * (uint32_t)(g_space_w - 1)) / (uint32_t)(fb.width - 1));
    }
    if (fb.height > 1 && g_space_h > 1) {
        ly = (int32_t)(((uint32_t)y * (uint32_t)(g_space_h - 1)) / (uint32_t)(fb.height - 1));
    }

    g_lx_fp = (lx << kFpShift);
    g_ly_fp = (ly << kFpShift);
}

void InitInput() {
    g_mx = 10;
    g_my = 10;
    g_mb = 0;
    g_sens_num = 1;
    g_sens_den = 1;
    if (kos::gfx::IsAvailable()) {
        const auto& fb = kos::gfx::GetInfo();
        g_space_w = (int)fb.width;
        g_space_h = (int)fb.height;
        g_lx_fp = (g_mx << kFpShift);
        g_ly_fp = (g_my << kFpShift);
    } else {
        g_space_w = 0;
        g_space_h = 0;
        g_lx_fp = 0;
        g_ly_fp = 0;
    }
}

void MouseMove(int dx, int dy) {
    if (!kos::gfx::IsAvailable()) return;
    EnsureDesktopSpace();
    // apply sensitivity scaling
    if (g_sens_den <= 0) g_sens_den = 1;
    g_lx_fp += (dx * g_sens_num * kFpOne) / g_sens_den;
    g_ly_fp += (dy * g_sens_num * kFpOne) / g_sens_den;
    ProjectLogicalToScreen();
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
    g_mx = x;
    g_my = y;
    ProjectScreenToLogical(x, y);
}

void SetMouseSensitivity(int num, int den) {
    if (num <= 0) num = 1; if (den <= 0) den = 1;
    g_sens_num = num; g_sens_den = den;
}

void SetMouseDesktopSpace(int width, int height) {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;
    if (width <= 0) width = (int)fb.width;
    if (height <= 0) height = (int)fb.height;

    g_space_w = width;
    g_space_h = height;
    ProjectScreenToLogical(g_mx, g_my);
    ProjectLogicalToScreen();
}

void GetMouseDesktopSpace(int& width, int& height) {
    EnsureDesktopSpace();
    width = g_space_w;
    height = g_space_h;
}

void AutoCalibrateMouseDesktopSpace() {
    if (!kos::gfx::IsAvailable()) return;
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;

    const int fbw = (int)fb.width;
    const int fbh = (int)fb.height;

    static const DesktopProfile kProfiles16x9[] = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
    };
    static const DesktopProfile kProfiles16x10[] = {
        {1280, 800}, {1440, 900}, {1680, 1050}, {1920, 1200}, {2560, 1600}
    };
    static const DesktopProfile kProfiles4x3[] = {
        {1024, 768}, {1280, 960}, {1400, 1050}, {1600, 1200}, {2048, 1536}
    };

    int targetW = fbw;
    int targetH = fbh;
    if (AspectNear(fbw, fbh, 16, 9)) {
        PickProfileAtLeast(kProfiles16x9, (int)(sizeof(kProfiles16x9) / sizeof(kProfiles16x9[0])), fbw, fbh, targetW, targetH);
    } else if (AspectNear(fbw, fbh, 16, 10)) {
        PickProfileAtLeast(kProfiles16x10, (int)(sizeof(kProfiles16x10) / sizeof(kProfiles16x10[0])), fbw, fbh, targetW, targetH);
    } else if (AspectNear(fbw, fbh, 4, 3)) {
        PickProfileAtLeast(kProfiles4x3, (int)(sizeof(kProfiles4x3) / sizeof(kProfiles4x3[0])), fbw, fbh, targetW, targetH);
    }

    SetMouseDesktopSpace(targetW, targetH);
}

bool LoadMouseDesktopSpaceConfig() {
    if (!kos::gfx::IsAvailable() || !kos::fs::g_fs_ptr) return false;

    uint8_t buf[128];
    int32_t n = kos::fs::g_fs_ptr->ReadFile(kMouseCfgPathUpper, buf, sizeof(buf));
    if (n <= 0) {
        n = kos::fs::g_fs_ptr->ReadFile(kMouseCfgPathLower, buf, sizeof(buf));
    }
    if (n <= 0) return false;

    int w = 0;
    int h = 0;
    if (!ParseMouseCfg(buf, (uint32_t)n, w, h)) return false;

    SetMouseDesktopSpace(w, h);
    return true;
}

bool SaveMouseDesktopSpaceConfig() {
    if (!kos::gfx::IsAvailable() || !kos::fs::g_fs_ptr) return false;

    int w = 0;
    int h = 0;
    GetMouseDesktopSpace(w, h);
    if (w <= 0 || h <= 0) return false;

    char out[64];
    int p = 0;
    auto put = [&](char c) {
        if (p < (int)sizeof(out) - 1) out[p++] = c;
    };
    auto putDec = [&](int v) {
        if (v <= 0) { put('0'); return; }
        char rev[12];
        int ri = 0;
        while (v > 0 && ri < (int)sizeof(rev)) {
            rev[ri++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (ri > 0) put(rev[--ri]);
    };

    putDec(w);
    put('x');
    putDec(h);
    put('\n');
    out[p] = 0;

    if (kos::fs::g_fs_ptr->WriteFile(kMouseCfgPathUpper, (const uint8_t*)out, (uint32_t)p)) return true;
    return kos::fs::g_fs_ptr->WriteFile(kMouseCfgPathLower, (const uint8_t*)out, (uint32_t)p);
}

}} // namespace
