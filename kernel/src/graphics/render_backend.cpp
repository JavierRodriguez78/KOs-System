#include <graphics/render_backend.hpp>

#include <graphics/gpu_accel.hpp>
#include <drivers/gpu/vmsvga.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <memory/heap.hpp>

namespace kos {
namespace gfx {
namespace render {

namespace {

using kos::common::uint8_t;
using kos::common::uint32_t;

struct BackendState {
    bool ready;
    bool gpu_backend;
    uint8_t bpp;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;
    uint8_t* framebuffer_base;
    uint32_t* backbuffer;
};

BackendState g_state = {false, false, 0, 0, 0, 0, nullptr, nullptr};

static inline uint32_t MinU32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

} // namespace

bool Initialize(uint32_t width,
                uint32_t height,
                uint32_t pitch_bytes,
                uint8_t bpp,
                uint8_t* framebuffer_base,
                bool gpu_backend) {
    g_state.ready = false;
    g_state.gpu_backend = gpu_backend;
    g_state.bpp = bpp;
    g_state.width = width;
    g_state.height = height;
    g_state.pitch_bytes = pitch_bytes;
    g_state.framebuffer_base = framebuffer_base;
    g_state.backbuffer = nullptr;

    const uint32_t bytes = width * height * 4u;
    g_state.backbuffer = (uint32_t*)kos::memory::Heap::Alloc(bytes, 4096);
    if (!g_state.backbuffer) {
        kos::console::Logger::Log("[RENDER] Backbuffer alloc failed");
        return false;
    }

    for (uint32_t i = 0; i < width * height; ++i) {
        g_state.backbuffer[i] = 0xFF000000u;
    }

    if (gpu_backend && kos::drivers::gpu::vmsvga::IsReady()) {
        kos::drivers::gpu::vmsvga::BindFramebuffer(framebuffer_base, width, height, pitch_bytes, bpp);
    }

    g_state.ready = true;
    return true;
}

void Shutdown() {
    g_state.ready = false;
    g_state.gpu_backend = false;
    g_state.framebuffer_base = nullptr;
    g_state.backbuffer = nullptr;
    g_state.width = 0;
    g_state.height = 0;
    g_state.pitch_bytes = 0;
    g_state.bpp = 0;
}

SurfaceView GetBackbuffer() {
    SurfaceView view = {g_state.backbuffer, g_state.width, g_state.height, g_state.width};
    return view;
}

void Fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_state.ready || !g_state.backbuffer) return;
    if (x >= g_state.width || y >= g_state.height) return;

    w = MinU32(w, g_state.width - x);
    h = MinU32(h, g_state.height - y);

    for (uint32_t row = 0; row < h; ++row) {
        uint32_t* dst = g_state.backbuffer + (y + row) * g_state.width + x;
        for (uint32_t col = 0; col < w; ++col) {
            dst[col] = color;
        }
    }

    if (g_state.gpu_backend && kos::drivers::gpu::vmsvga::IsReady()) {
        kos::drivers::gpu::vmsvga::FillRect(x, y, w, h, color);
    }
}

void Blit(uint32_t src_x,
          uint32_t src_y,
          uint32_t dst_x,
          uint32_t dst_y,
          uint32_t w,
          uint32_t h) {
    if (!g_state.ready || !g_state.backbuffer) return;
    if (src_x >= g_state.width || src_y >= g_state.height) return;
    if (dst_x >= g_state.width || dst_y >= g_state.height) return;

    const uint32_t copy_w = MinU32(w, MinU32(g_state.width - src_x, g_state.width - dst_x));
    const uint32_t copy_h = MinU32(h, MinU32(g_state.height - src_y, g_state.height - dst_y));
    if (copy_w == 0 || copy_h == 0) return;

    if (dst_y > src_y) {
        for (uint32_t row = copy_h; row > 0; --row) {
            uint32_t* dst = g_state.backbuffer + (dst_y + row - 1u) * g_state.width + dst_x;
            uint32_t* src = g_state.backbuffer + (src_y + row - 1u) * g_state.width + src_x;
            kos::lib::String::memmove(dst, src, copy_w * 4u);
        }
    } else {
        for (uint32_t row = 0; row < copy_h; ++row) {
            uint32_t* dst = g_state.backbuffer + (dst_y + row) * g_state.width + dst_x;
            uint32_t* src = g_state.backbuffer + (src_y + row) * g_state.width + src_x;
            kos::lib::String::memmove(dst, src, copy_w * 4u);
        }
    }

    if (g_state.gpu_backend && kos::drivers::gpu::vmsvga::IsReady()) {
        kos::drivers::gpu::vmsvga::BlitRect(src_x, src_y, dst_x, dst_y, copy_w, copy_h);
    }
}

void Upload(const uint32_t* src,
            uint32_t src_width,
            uint32_t src_height,
            uint32_t src_pitch_pixels,
            uint32_t dst_x,
            uint32_t dst_y) {
    if (!g_state.ready || !g_state.backbuffer || !src) return;
    if (dst_x >= g_state.width || dst_y >= g_state.height) return;

    const uint32_t copy_w = MinU32(src_width, g_state.width - dst_x);
    const uint32_t copy_h = MinU32(src_height, g_state.height - dst_y);
    for (uint32_t row = 0; row < copy_h; ++row) {
        uint32_t* dst = g_state.backbuffer + (dst_y + row) * g_state.width + dst_x;
        const uint32_t* src_row = src + row * src_pitch_pixels;
        kos::lib::String::memmove(dst, src_row, copy_w * 4u);
    }

    if (g_state.gpu_backend && kos::drivers::gpu::vmsvga::IsReady()) {
        kos::drivers::gpu::vmsvga::UploadRect(src, src_width, src_height, src_pitch_pixels, dst_x, dst_y);
    }
}

void Present() {
    if (!g_state.ready || !g_state.framebuffer_base || !g_state.backbuffer) return;

    if (g_state.gpu_backend && kos::drivers::gpu::vmsvga::IsReady()) {
        kos::drivers::gpu::vmsvga::Present();
        return;
    }

    if (g_state.bpp == 32) {
        for (uint32_t y = 0; y < g_state.height; ++y) {
            uint32_t* dst = (uint32_t*)(g_state.framebuffer_base + y * g_state.pitch_bytes);
            uint32_t* src = g_state.backbuffer + y * g_state.width;
            kos::lib::String::memmove(dst, src, g_state.width * 4u);
        }
    } else {
        for (uint32_t y = 0; y < g_state.height; ++y) {
            uint8_t* dst = g_state.framebuffer_base + y * g_state.pitch_bytes;
            uint32_t* src = g_state.backbuffer + y * g_state.width;
            for (uint32_t x = 0; x < g_state.width; ++x) {
                const uint32_t argb = src[x];
                dst[x * 3u + 0u] = (uint8_t)(argb & 0xFFu);
                dst[x * 3u + 1u] = (uint8_t)((argb >> 8) & 0xFFu);
                dst[x * 3u + 2u] = (uint8_t)((argb >> 16) & 0xFFu);
            }
        }
    }

}

} // namespace render
} // namespace gfx
} // namespace kos