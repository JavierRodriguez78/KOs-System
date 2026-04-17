#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <memory/paging.hpp>
#include <memory/memory.hpp>
#include <memory/heap.hpp>
#include <ui/input.hpp>
#include <graphics/font8x8_basic.hpp>
#include <drivers/mouse/mouse_stats.hpp>

using namespace kos::gfx;
using namespace kos::lib;

namespace kos { namespace gfx {

bool Compositor::s_ready = false;
uint32_t* Compositor::s_backbuf = nullptr;
uint32_t Compositor::s_pitchBytes = 0;
uint32_t Compositor::s_width = 0;
uint32_t Compositor::s_height = 0;
static uint8_t* s_fbBase = nullptr; // mapped framebuffer base (virtual)
static bool s_clip_enabled = false;
static Rect s_clip_rect = {0, 0, 0, 0};

static inline uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool Compositor::Initialize() {
    if (!gfx::IsAvailable()) return false;
    const auto& fb = gfx::GetInfo();
    if (fb.type != 1 || (fb.bpp != 32 && fb.bpp != 24)) return false;
    s_width = fb.width; s_height = fb.height; s_pitchBytes = fb.pitch;
    // Map framebuffer if it's not identity-mapped (< 64 MiB)
    const uint32_t ID_MAP_END = 64 * 1024 * 1024;
    uint32_t fbBytes = s_height * s_pitchBytes;
    // round up to page size
    uint32_t mapSize = (fbBytes + 4095) & ~4095u;
    if (fb.addr < ID_MAP_END) {
        s_fbBase = (uint8_t*)(uint32_t)fb.addr;
    } else {
        // Map to a fixed virtual address range
        const uint32_t VIRT_FB_BASE = 0x10000000u; // 256 MiB
    kos::memory::Paging::MapRange((virt_addr_t)VIRT_FB_BASE,
                      (phys_addr_t)fb.addr,
                                      mapSize,
                                      kos::memory::Paging::Present | kos::memory::Paging::RW | kos::memory::Paging::WriteThrough);
        s_fbBase = (uint8_t*)VIRT_FB_BASE;
    }
    // Allocate a simple backbuffer: width*height*4 bytes via kernel heap
    uint32_t bbBytes = s_width * s_height * 4;
    s_backbuf = (uint32_t*)kos::memory::Heap::Alloc(bbBytes, 4096);
    if (!s_backbuf) {
        kos::console::Logger::Log("[COMPOSITOR] Backbuffer alloc failed");
        s_ready = false;
        return false;
    }
    // Initialize to transparent black
    for (uint32_t i = 0; i < s_width * s_height; ++i) s_backbuf[i] = 0xFF000000u;
    s_ready = true;
    // Basic diagnostics
    kos::console::Logger::Log("[COMPOSITOR] initialized");
    return true;
}

void Compositor::Shutdown() {
    s_ready = false;
    s_backbuf = nullptr;
}

void Compositor::BeginFrame(uint32_t wallpaper) {
    if (!s_ready) return;
    s_clip_enabled = false;
    // Clear backbuffer to wallpaper color
    for (uint32_t i = 0; i < s_width * s_height; ++i) s_backbuf[i] = wallpaper;
}

static inline void blitRect(uint32_t* dst, uint32_t pitchPixels, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t* row = dst + (y + j) * pitchPixels + x;
        for (uint32_t i = 0; i < w; ++i) row[i] = color;
    }
}

static inline uint8_t addSat8(uint8_t v, int delta) {
    int nv = (int)v + delta;
    if (nv < 0) nv = 0;
    if (nv > 255) nv = 255;
    return (uint8_t)nv;
}

static inline uint32_t darkenColor(uint32_t c, uint8_t amount) {
    uint8_t r = (uint8_t)((c >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((c >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(c & 0xFFu);
    r = (r > amount) ? (uint8_t)(r - amount) : 0;
    g = (g > amount) ? (uint8_t)(g - amount) : 0;
    b = (b > amount) ? (uint8_t)(b - amount) : 0;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline uint32_t tintColor(uint32_t c, int delta) {
    uint8_t r = (uint8_t)((c >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((c >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(c & 0xFFu);
    r = addSat8(r, delta);
    g = addSat8(g, delta);
    b = addSat8(b, delta);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void blitChecker(uint32_t* dst, uint32_t pitchPixels,
                               uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                               uint32_t cA, uint32_t cB, uint32_t cell = 2) {
    if (cell == 0) cell = 1;
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t* row = dst + (y + j) * pitchPixels + x;
        for (uint32_t i = 0; i < w; ++i) {
            uint32_t tx = (x + i) / cell;
            uint32_t ty = (y + j) / cell;
            row[i] = ((tx + ty) & 1u) ? cA : cB;
        }
    }
}

void Compositor::DrawWindow(const WindowDesc& win) {
    if (!s_ready) return;
    // Clamp to screen
    uint32_t x = win.x, y = win.y, w = win.w, h = win.h;
    if (x >= s_width || y >= s_height) return;
    if (x + w > s_width) w = s_width - x;
    if (y + h > s_height) h = s_height - y;

    // Title bar height
    const uint32_t th = 18; // keep in sync with ui::TitleBarHeight()
    const uint32_t stride = s_width; // backbuffer stride in pixels
    // Retro 8-bit palette for global window chrome.
    const uint32_t kTitleBg = 0xFF1A3470u;
    const uint32_t kTitleFg = 0xFFFFFF9Eu;
    const uint32_t kTitleShadow = 0xFF0A1020u;
    const uint32_t kTopLight = 0xFF9CC7FFu;
    const uint32_t kBottomDark = 0xFF001636u;
    // Draw body (guard against small heights)
    uint32_t bodyH = (h > th) ? (h - th) : 0;
    if (bodyH) {
        uint32_t bodyA = tintColor(win.bg, -8);
        uint32_t bodyB = tintColor(win.bg, 6);
        blitChecker(s_backbuf, stride, x, y + th, w, bodyH, bodyA, bodyB, 2);
    }
    // Draw title bar
    blitRect(s_backbuf, stride, x, y, w, (h < th ? h : th), kTitleBg);
    // Pixel stripes for 8-bit feel.
    for (uint32_t sy = y + 1; sy < y + (h < th ? h : th); sy += 2) {
        blitRect(s_backbuf, stride, x, sy, w, 1, 0xFF17305Fu);
    }
    // Simple top border
    blitRect(s_backbuf, stride, x, y, w, 1, kTopLight);
    // Left/Right/Bottom borders
    blitRect(s_backbuf, stride, x, y, 1, h, kTopLight);
    if (w > 1) blitRect(s_backbuf, stride, x + w - 1, y, 1, h, kBottomDark);
    if (h > 1) blitRect(s_backbuf, stride, x, y + h - 1, w, 1, kBottomDark);
    // Title text: for now, omitted (no font rasterizer available yet)
    if (win.title) {
        // Render title using 8x8 font in the title bar, with small padding
        const uint32_t padX = 4, padY = 4;
        uint32_t maxChars = (w > padX*2 ? (w - padX*2) / 8 : 0);
        if (maxChars) {
            for (uint32_t i = 0; win.title[i] && i < maxChars; ++i) {
                char ch = win.title[i];
                if (ch < 32 || ch > 127) ch = '?';
                const uint8_t* glyph = kFont8x8Basic[ch - 32];
                DrawGlyph8x8(x + padX + i*8 + 1, y + padY + 1, glyph, kTitleShadow, kTitleBg);
                DrawGlyph8x8(x + padX + i*8, y + padY, glyph, kTitleFg, kTitleBg);
            }
        }
    }
    // Window control buttons (Close/Max/Min) simplistic glyphs
    // For now we always draw them; UI framework decides interactivity.
    const uint32_t btnW = 18; const uint32_t pad = 2; const uint32_t btnH = (h < th ? h : th);
    uint32_t closeX = x + w - pad - btnW; uint32_t closeY = y;
    uint32_t maxX = (closeX >= btnW+pad ? closeX - (btnW+pad) : closeX); uint32_t maxY = y;
    uint32_t minX = (maxX >= btnW+pad ? maxX - (btnW+pad) : maxX); uint32_t minY = y;
    auto drawBtn=[&](uint32_t bx,uint32_t by,uint32_t color){
        // background
        blitRect(s_backbuf, stride, bx, by, btnW, btnH, 0xFF244684u);
        // border
        blitRect(s_backbuf, stride, bx, by, btnW, 1, kTopLight);
        blitRect(s_backbuf, stride, bx, by+btnH-1, btnW, 1, kBottomDark);
        blitRect(s_backbuf, stride, bx, by, 1, btnH, kTopLight);
        blitRect(s_backbuf, stride, bx+btnW-1, by, 1, btnH, kBottomDark);
        // simple glyph (center lines)
        uint32_t gx = bx + (btnW/2) - 4; uint32_t gy = by + (btnH/2) - 4;
        for (uint32_t r=0;r<8;++r){
            for(uint32_t c=0;c<8;++c){
                bool on=false;
                // pattern depends on color meaning we pass code for which button
                if (color==0){ // min: horizontal line in middle
                    on = (r==3);
                } else if (color==1){ // max: rectangle outline
                    on = (r==0||r==7||c==0||c==7);
                } else { // close: X
                    on = (c==r||c==7-r);
                }
                s_backbuf[(gy+r)*stride + (gx+c)] = on ? 0xFFFFE26Fu : 0xFF244684u;
            }
        }
    };
    drawBtn(minX, minY, 0); // minimize
    drawBtn(maxX, maxY, 1); // maximize
    drawBtn(closeX, closeY, 2); // close
}

void Compositor::EndFrame() {
    if (!s_ready) return;
    // Subtle CRT scanlines: darken every other row before presenting.
    for (uint32_t y = 1; y < s_height; y += 2) {
        uint32_t* row = s_backbuf + y * s_width;
        for (uint32_t x = 0; x < s_width; ++x) {
            row[x] = darkenColor(row[x], 18);
        }
    }

    // Copy backbuffer to framebuffer. Use the mapped base (identity-mapped when below 64MiB,
    // otherwise explicitly mapped in Initialize). Writing directly to the physical address can
    // fault on platforms where the framebuffer sits above the identity map.
    const auto& fb = gfx::GetInfo();
    uint8_t* dstBase = s_fbBase;
    if (!dstBase) return;
    // Optionally warp slightly from screen edges for a smoother experience
    // (no-op right now; we just render the cursor)
    if (fb.bpp == 32) {
        for (uint32_t y = 0; y < s_height; ++y) {
            uint32_t* dst = (uint32_t*)(dstBase + y * s_pitchBytes);
            uint32_t* src = s_backbuf + y * s_width;
            for (uint32_t x = 0; x < s_width; ++x) dst[x] = src[x];
        }
    } else { // 24 bpp BGR
        for (uint32_t y = 0; y < s_height; ++y) {
            uint8_t* dst = (uint8_t*)(dstBase + y * s_pitchBytes);
            uint32_t* src = s_backbuf + y * s_width;
            for (uint32_t x = 0; x < s_width; ++x) {
                uint32_t argb = src[x];
                uint8_t r = (argb >> 16) & 0xFF;
                uint8_t g = (argb >> 8) & 0xFF;
                uint8_t b = (argb) & 0xFF;
                uint8_t* p = dst + x * 3;
                p[0] = b; p[1] = g; p[2] = r;
            }
        }
    }
}

void Compositor::DrawGlyph8x8(uint32_t x, uint32_t y, const uint8_t glyph[8], uint32_t fgARGB, uint32_t bgARGB) {
    if (!s_ready) return;
    if (x >= s_width || y >= s_height) return;
    const uint32_t stride = s_width;
    for (uint32_t row=0; row<8; ++row) {
        uint32_t py = y + row;
        if (py >= s_height) break;
        uint8_t bits = glyph[row];
        uint32_t* dst = s_backbuf + py * stride;
        for (uint32_t col=0; col<8; ++col) {
            uint32_t px = x + col;
            if (px >= s_width) break;
            if (s_clip_enabled) {
                if (px < s_clip_rect.x || py < s_clip_rect.y ||
                    px >= s_clip_rect.x + s_clip_rect.w || py >= s_clip_rect.y + s_clip_rect.h) {
                    continue;
                }
            }
            // Many 8x8 public-domain fonts encode leftmost pixel in bit0 (LSB-left).
            // Use LSB-left mapping to avoid horizontally mirrored glyphs.
            bool on = (bits & (0x01u << col)) != 0;
            dst[px] = on ? fgARGB : bgARGB;
        }
    }
}

void Compositor::FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!s_ready) return;
    if (x >= s_width || y >= s_height) return;
    if (x + w > s_width) w = s_width - x;
    if (y + h > s_height) h = s_height - y;

    if (s_clip_enabled) {
        uint32_t cx1 = s_clip_rect.x;
        uint32_t cy1 = s_clip_rect.y;
        uint32_t cx2 = s_clip_rect.x + s_clip_rect.w;
        uint32_t cy2 = s_clip_rect.y + s_clip_rect.h;
        uint32_t rx1 = x;
        uint32_t ry1 = y;
        uint32_t rx2 = x + w;
        uint32_t ry2 = y + h;
        if (rx2 <= cx1 || ry2 <= cy1 || rx1 >= cx2 || ry1 >= cy2) return;
        if (rx1 < cx1) rx1 = cx1;
        if (ry1 < cy1) ry1 = cy1;
        if (rx2 > cx2) rx2 = cx2;
        if (ry2 > cy2) ry2 = cy2;
        x = rx1;
        y = ry1;
        w = rx2 - rx1;
        h = ry2 - ry1;
    }

    const uint32_t stride = s_width;
    for (uint32_t j=0;j<h;++j) {
        uint32_t* row = s_backbuf + (y + j) * stride + x;
        for (uint32_t i=0;i<w;++i) row[i] = color;
    }
}

void Compositor::SetClipRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!s_ready || w == 0 || h == 0) {
        s_clip_enabled = false;
        return;
    }
    if (x >= s_width || y >= s_height) {
        s_clip_enabled = false;
        return;
    }
    if (x + w > s_width) w = s_width - x;
    if (y + h > s_height) h = s_height - y;
    s_clip_rect.x = x;
    s_clip_rect.y = y;
    s_clip_rect.w = w;
    s_clip_rect.h = h;
    s_clip_enabled = true;
}

void Compositor::ClearClipRect() {
    s_clip_enabled = false;
}

}}
