#pragma once
#include <common/types.hpp>

namespace kos { namespace kernel {

class MultibootKernel {
public:
    MultibootKernel(const void* mb_info, kos::common::uint32_t magic);
    // Initialize parsing and any multiboot-driven subsystems (framebuffer)
    void Init();

    kos::common::uint8_t MousePollMode() const { return mousePollMode; }
    kos::common::uint32_t MemLowerKB() const { return memLowerKB; }
    kos::common::uint32_t MemUpperKB() const { return memUpperKB; }

private:
    const void* mb_info;
    kos::common::uint32_t magic;
    kos::common::uint8_t mousePollMode;
    kos::common::uint32_t memLowerKB;
    kos::common::uint32_t memUpperKB;
};

} }
