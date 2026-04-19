#ifndef KOS_GRAPHICS_GPU_ACCEL_HPP
#define KOS_GRAPHICS_GPU_ACCEL_HPP

#include <common/types.hpp>

namespace kos {
namespace gfx {
namespace gpu {

struct GpuDeviceInfo {
    bool probed;
    bool present;
    bool compatible;
    bool bar0_is_io;

    kos::common::uint16_t vendor_id;
    kos::common::uint16_t device_id;
    kos::common::uint8_t class_id;
    kos::common::uint8_t subclass_id;
    kos::common::uint8_t interface_id;

    kos::common::uint16_t bus;
    kos::common::uint16_t device;
    kos::common::uint16_t function;

    kos::common::uint32_t bar0;
    kos::common::uint32_t mmio_bar0;
};

// Probe PCI and cache the first/best display controller found.
void ProbePCI();

bool IsProbed();
bool HasDevice();
bool IsAccelerationEnabled();
bool InitializeBackend(kos::common::uint32_t width, kos::common::uint32_t height);
void PresentFrame(kos::common::uint32_t width, kos::common::uint32_t height);

const GpuDeviceInfo& GetDeviceInfo();
const char* ActiveBackendName();

} // namespace gpu
} // namespace gfx
} // namespace kos

#endif // KOS_GRAPHICS_GPU_ACCEL_HPP