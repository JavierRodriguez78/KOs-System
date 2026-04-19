#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <memory/paging.hpp>
#include <memory/memory.hpp>
#include <memory/heap.hpp>
#include <ui/input.hpp>
#include <graphics/font8x8_basic.hpp>
#include <graphics/gpu_accel.hpp>
#include <graphics/render_backend.hpp>
#include <drivers/mouse/mouse_stats.hpp>

using namespace kos::gfx;
using namespace kos::lib;

namespace kos { namespace gfx {

bool Compositor::s_ready = false;
bool Compositor::s_gpu_backend = false;
uint32_t Compositor::s_pitchBytes = 0;
uint32_t Compositor::s_width = 0;
uint32_t Compositor::s_height = 0;
static uint8_t* s_fbBase = nullptr; // mapped framebuffer base (virtual)
static bool s_clip_enabled = false;
static Rect s_clip_rect = {0, 0, 0, 0};

static inline uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline uint32_t* BackbufferPixels() {
    return kos::gfx::render::GetBackbuffer().pixels;
}

bool Compositor::Initialize() {
    if (!gfx::IsAvailable()) return false;
    gpu::ProbePCI();
    const auto& fb = gfx::GetInfo();
    if (fb.type != 1 || (fb.bpp != 32 && fb.bpp != 24)) return false;
    s_width = fb.width; s_height = fb.height; s_pitchBytes = fb.pitch;
    s_gpu_backend = gpu::InitializeBackend(s_width, s_height);
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
    if (!kos::gfx::render::Initialize(s_width, s_height, s_pitchBytes, fb.bpp, s_fbBase, s_gpu_backend)) {
        kos::console::Logger::Log("[COMPOSITOR] render backend init failed");
        s_ready = false;
        return false;
    }
    s_ready = true;
    // Basic diagnostics
    kos::console::Logger::Log("[COMPOSITOR] initialized");
    kos::console::Logger::LogKV("[COMPOSITOR] backend", BackendName());
    return true;
}

void Compositor::Shutdown() {
    s_ready = false;
    s_gpu_backend = false;
    kos::gfx::render::Shutdown();
}

bool Compositor::IsUsingGpuBackend() { return s_gpu_backend; }

const char* Compositor::BackendName() { return s_gpu_backend ? gpu::ActiveBackendName() : "cpu"; }

void Compositor::BeginFrame(uint32_t wallpaper) {
    if (!s_ready) return;
    s_clip_enabled = false;
    kos::gfx::render::Fill(0, 0, s_width, s_height, wallpaper);
}

static inline void blitRect(uint32_t* dst, uint32_t pitchPixels, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t* row = dst + (y + j) * pitchPixels + x;
        for (uint32_t i = 0; i < w; ++i) row[i] = color;
    }
}

static inline void blitRectBackend(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    kos::gfx::render::Fill(x, y, w, h, color);
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

static inline void blitCheckerBackend(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                      uint32_t cA, uint32_t cB, uint32_t cell = 2) {
    if (cell == 0) cell = 1;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const uint32_t tx = (x + i) / cell;
            const uint32_t ty = (y + j) / cell;
            kos::gfx::render::Fill(x + i, y + j, 1, 1, ((tx + ty) & 1u) ? cA : cB);
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
        blitCheckerBackend(x, y + th, w, bodyH, bodyA, bodyB, 2);
    }
    // Draw title bar
    blitRectBackend(x, y, w, (h < th ? h : th), kTitleBg);
    // Pixel stripes for 8-bit feel.
    for (uint32_t sy = y + 1; sy < y + (h < th ? h : th); sy += 2) {
        blitRectBackend(x, sy, w, 1, 0xFF17305Fu);
    }
    // Simple top border
    blitRectBackend(x, y, w, 1, kTopLight);
    // Left/Right/Bottom borders
    blitRectBackend(x, y, 1, h, kTopLight);
    if (w > 1) blitRectBackend(x + w - 1, y, 1, h, kBottomDark);
    if (h > 1) blitRectBackend(x, y + h - 1, w, 1, kBottomDark);
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
        blitRectBackend(bx, by, btnW, btnH, 0xFF244684u);
        // border
        blitRectBackend(bx, by, btnW, 1, kTopLight);
        blitRectBackend(bx, by+btnH-1, btnW, 1, kBottomDark);
        blitRectBackend(bx, by, 1, btnH, kTopLight);
        blitRectBackend(bx+btnW-1, by, 1, btnH, kBottomDark);
        // simple glyph (center lines)
        uint32_t gx = bx + (btnW/2) - 4; uint32_t gy = by + (btnH/2) - 4;
        uint32_t glyphBuf[64];
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
                glyphBuf[r * 8 + c] = on ? 0xFFFFE26Fu : 0xFF244684u;
            }
        }
        kos::gfx::render::Upload(glyphBuf, 8, 8, 8, gx, gy);
    };
    drawBtn(minX, minY, 0); // minimize
    drawBtn(maxX, maxY, 1); // maximize
    drawBtn(closeX, closeY, 2); // close
}

void Compositor::EndFrame() {
    if (!s_ready) return;
    // Keep scanline post-process for CPU backend only.
    if (!s_gpu_backend) {
        uint32_t* backbuf = BackbufferPixels();
        for (uint32_t y = 1; y < s_height; y += 2) {
            uint32_t* row = backbuf + y * s_width;
            for (uint32_t x = 0; x < s_width; ++x) {
                row[x] = darkenColor(row[x], 18);
            }
        }
    }
    kos::gfx::render::Present();
}

void Compositor::DrawGlyph8x8(uint32_t x, uint32_t y, const uint8_t glyph[8], uint32_t fgARGB, uint32_t bgARGB) {
    if (!s_ready) return;
    if (x >= s_width || y >= s_height) return;
    uint32_t glyphBuf[64];
    for (uint32_t row=0; row<8; ++row) {
        uint32_t py = y + row;
        if (py >= s_height) break;
        uint8_t bits = glyph[row];
        for (uint32_t col=0; col<8; ++col) {
            uint32_t px = x + col;
            if (px >= s_width) break;
            if (s_clip_enabled) {
                if (px < s_clip_rect.x || py < s_clip_rect.y ||
                    px >= s_clip_rect.x + s_clip_rect.w || py >= s_clip_rect.y + s_clip_rect.h) {
                    glyphBuf[row * 8 + col] = 0;
                    continue;
                }
            }
            // Many 8x8 public-domain fonts encode leftmost pixel in bit0 (LSB-left).
            // Use LSB-left mapping to avoid horizontally mirrored glyphs.
            bool on = (bits & (0x01u << col)) != 0;
            glyphBuf[row * 8 + col] = on ? fgARGB : bgARGB;
        }
    }
    kos::gfx::render::Upload(glyphBuf, 8, 8, 8, x, y);
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

    kos::gfx::render::Fill(x, y, w, h, color);
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
