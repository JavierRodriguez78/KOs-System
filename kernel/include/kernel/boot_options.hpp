#pragma once
#include <common/types.hpp>

namespace kos { namespace kernel {

enum class DisplayMode : kos::common::uint8_t {
    Text = 0,
    Graphics = 1
};

class BootOptions {
public:
    static BootOptions ParseFromMultiboot(const void* mb_info, kos::common::uint32_t magic);

    kos::common::uint8_t MousePollMode() const { return mousePollMode; }
    bool DebugEnabled() const { return debugEnabled; }
    bool RebootOnPanic() const { return rebootOnPanic; }
    DisplayMode Mode() const { return mode; }

private:
    BootOptions();
    kos::common::uint8_t mousePollMode;
    bool debugEnabled;
    bool rebootOnPanic;
    DisplayMode mode;
};

} }