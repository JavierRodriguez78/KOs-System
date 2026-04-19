#ifndef KOS_GRAPHICS_RENDER_BACKEND_HPP
#define KOS_GRAPHICS_RENDER_BACKEND_HPP

#include <common/types.hpp>

namespace kos {
namespace gfx {
namespace render {

struct SurfaceView {
    kos::common::uint32_t* pixels;
    kos::common::uint32_t width;
    kos::common::uint32_t height;
    kos::common::uint32_t pitch_pixels;
};

bool Initialize(kos::common::uint32_t width,
                kos::common::uint32_t height,
                kos::common::uint32_t pitch_bytes,
                kos::common::uint8_t bpp,
                kos::common::uint8_t* framebuffer_base,
                bool gpu_backend);

void Shutdown();

SurfaceView GetBackbuffer();

void Fill(kos::common::uint32_t x,
          kos::common::uint32_t y,
          kos::common::uint32_t w,
          kos::common::uint32_t h,
          kos::common::uint32_t color);

void Blit(kos::common::uint32_t src_x,
          kos::common::uint32_t src_y,
          kos::common::uint32_t dst_x,
          kos::common::uint32_t dst_y,
          kos::common::uint32_t w,
          kos::common::uint32_t h);

void Upload(const kos::common::uint32_t* src,
            kos::common::uint32_t src_width,
            kos::common::uint32_t src_height,
            kos::common::uint32_t src_pitch_pixels,
            kos::common::uint32_t dst_x,
            kos::common::uint32_t dst_y);

void Present();

} // namespace render
} // namespace gfx
} // namespace kos

#endif // KOS_GRAPHICS_RENDER_BACKEND_HPP