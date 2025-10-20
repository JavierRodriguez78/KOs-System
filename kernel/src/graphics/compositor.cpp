#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <lib/string.hpp>
#include <memory/paging.hpp>
#include <memory/memory.hpp>
#include <memory/heap.hpp>
#include <ui/input.hpp>

using namespace kos::gfx;
using namespace kos::lib;

namespace kos { namespace gfx {

bool Compositor::s_ready = false;
uint32_t* Compositor::s_backbuf = nullptr;
uint32_t Compositor::s_pitchBytes = 0;
uint32_t Compositor::s_width = 0;
uint32_t Compositor::s_height = 0;
static uint8_t* s_fbBase = nullptr; // mapped framebuffer base (virtual)

static inline uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool Compositor::Initialize() {
    if (!gfx::IsAvailable()) return false;
    const auto& fb = gfx::GetInfo();
    if (fb.bpp != 32 || fb.type != 1) return false;
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
        s_ready = false;
        return false;
    }
    // Initialize to transparent black
    for (uint32_t i = 0; i < s_width * s_height; ++i) s_backbuf[i] = 0xFF000000u;
    s_ready = true;
    return true;
}

void Compositor::Shutdown() {
    s_ready = false;
    s_backbuf = nullptr;
}

void Compositor::BeginFrame(uint32_t wallpaper) {
    if (!s_ready) return;
    // Clear backbuffer to wallpaper color
    for (uint32_t i = 0; i < s_width * s_height; ++i) s_backbuf[i] = wallpaper;
}

static inline void blitRect(uint32_t* dst, uint32_t pitchPixels, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t* row = dst + (y + j) * pitchPixels + x;
        for (uint32_t i = 0; i < w; ++i) row[i] = color;
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
    const uint32_t th = 18;
    const uint32_t stride = s_width; // backbuffer stride in pixels
    // Draw body (guard against small heights)
    uint32_t bodyH = (h > th) ? (h - th) : 0;
    if (bodyH) blitRect(s_backbuf, stride, x, y + th, w, bodyH, win.bg);
    // Draw title bar
    blitRect(s_backbuf, stride, x, y, w, (h < th ? h : th), 0xFF2D2D30u);
    // Simple top border
    blitRect(s_backbuf, stride, x, y, w, 1, 0xFFFFFFFFu);
    // Left/Right/Bottom borders
    blitRect(s_backbuf, stride, x, y, 1, h, 0xFFFFFFFFu);
    if (w > 1) blitRect(s_backbuf, stride, x + w - 1, y, 1, h, 0xFF000000u);
    if (h > 1) blitRect(s_backbuf, stride, x, y + h - 1, w, 1, 0xFF000000u);
    // Title text: for now, omitted (no font rasterizer available yet)
}

void Compositor::EndFrame() {
    if (!s_ready) return;
    // Copy backbuffer to framebuffer (mapped at s_fbBase)
    if (!s_fbBase) return;
    // Optionally warp slightly from screen edges for a smoother experience
    // (no-op right now; we just render the cursor)
    // Draw a very simple mouse cursor (10x16) into backbuffer before presenting
    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    const uint32_t stride = s_width;
    // Triangle cursor pattern
    for (int j = 0; j < 16; ++j) {
        for (int i = 0; i <= j && i < 10; ++i) {
            int x = mx + i; int y = my + j;
            if (x >= 0 && y >= 0 && (uint32_t)x < s_width && (uint32_t)y < s_height) {
                uint32_t color = 0xFFFFFFFFu; // white
                if (mb & 1u) color = 0xFFFFA500u; // left pressed: orange
                s_backbuf[y * stride + x] = color;
            }
        }
    }
    for (uint32_t y = 0; y < s_height; ++y) {
        uint32_t* dst = (uint32_t*)(s_fbBase + y * s_pitchBytes);
        uint32_t* src = s_backbuf + y * s_width;
        // Copy width pixels
        for (uint32_t x = 0; x < s_width; ++x) dst[x] = src[x];
    }
}

}}
