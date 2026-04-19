#ifndef KOS_DRIVERS_GPU_VMSVGA_HPP
#define KOS_DRIVERS_GPU_VMSVGA_HPP

#include <common/types.hpp>
#include <graphics/gpu_accel.hpp>

namespace kos {
namespace drivers {
namespace gpu {
namespace vmsvga {

bool Initialize(const kos::gfx::gpu::GpuDeviceInfo& dev,
                kos::common::uint32_t width,
                kos::common::uint32_t height);

bool IsReady();

void BindFramebuffer(kos::common::uint8_t* framebuffer_base,
                     kos::common::uint32_t width,
                     kos::common::uint32_t height,
                     kos::common::uint32_t pitch_bytes,
                     kos::common::uint8_t bpp);

void FillRect(kos::common::uint32_t x,
              kos::common::uint32_t y,
              kos::common::uint32_t w,
              kos::common::uint32_t h,
              kos::common::uint32_t color);

void BlitRect(kos::common::uint32_t src_x,
              kos::common::uint32_t src_y,
              kos::common::uint32_t dst_x,
              kos::common::uint32_t dst_y,
              kos::common::uint32_t w,
              kos::common::uint32_t h);

void UploadRect(const kos::common::uint32_t* src,
                kos::common::uint32_t src_width,
                kos::common::uint32_t src_height,
                kos::common::uint32_t src_pitch_pixels,
                kos::common::uint32_t dst_x,
                kos::common::uint32_t dst_y);

void PresentRect(kos::common::uint32_t x,
                 kos::common::uint32_t y,
                 kos::common::uint32_t w,
                 kos::common::uint32_t h);

void Present();

} // namespace vmsvga
} // namespace gpu
} // namespace drivers
} // namespace kos

#endif // KOS_DRIVERS_GPU_VMSVGA_HPP
