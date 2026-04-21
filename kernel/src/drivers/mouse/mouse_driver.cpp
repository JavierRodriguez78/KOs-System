//View https://wiki.osdev.org/Mouse_Input

#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/mouse/mouse_constants.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>
#include <kernel/input_debug.hpp>
#include <lib/serial.hpp>
#include <graphics/framebuffer.hpp>
#include <ui/input.hpp>
#include <input/event_queue.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers;
using namespace kos::drivers::mouse;

namespace {
static inline void enqueue_mouse_event(kos::input::EventType type) {
    int mx = 0, my = 0;
    uint8_t buttons = 0;
    kos::ui::GetMouseState(mx, my, buttons);

    kos::input::InputEvent ev{};
    ev.type = type;
    ev.timestamp_ms = 0;
    ev.target_window = 0;
    ev.mouse_data.x = mx;
    ev.mouse_data.y = my;
    ev.mouse_data.buttons = buttons;
    (void)kos::input::InputEventQueue::Instance().Enqueue(ev);
}

enum class MouseDecodeMode : uint8_t {
    Auto = 0,
    HeaderSign = 1,
    SignedByte = 2
};

static MouseDecodeMode s_decode_mode = MouseDecodeMode::Auto;
struct DecodeCandidate {
    int32_t x;
    int32_t y;
    uint32_t edgeHits;
    uint32_t travel;
    uint32_t edgeHitsWindow;
    uint32_t travelWindow;
    uint32_t edgeHitsWindowSlow;
    uint32_t travelWindowSlow;
    uint32_t edgeHitsWindowFast;
    uint32_t travelWindowFast;
};
static DecodeCandidate s_decode[2];
static uint32_t s_decode_packets = 0;
static uint32_t s_decode_window_packets = 0;
static uint32_t s_decode_window_slow_packets = 0;
static uint32_t s_decode_window_fast_packets = 0;
static uint32_t s_decode_pending_idx = 0;
static uint32_t s_decode_pending_windows = 0;

enum class MouseOrientMode : uint8_t {
    Auto = 0,
    X_Y = 1,
    NX_Y = 2,
    X_NY = 3,
    NX_NY = 4,
    Y_X = 5,
    NY_X = 6,
    Y_NX = 7,
    NY_NX = 8
};

struct OrientCandidate {
    int32_t x;
    int32_t y;
    uint32_t edgeHits;
    uint32_t travel;
    uint32_t edgeHitsWindow;
    uint32_t travelWindow;
    uint32_t edgeHitsWindowSlow;
    uint32_t travelWindowSlow;
    uint32_t edgeHitsWindowFast;
    uint32_t travelWindowFast;
};

static MouseOrientMode s_orient_mode = MouseOrientMode::Auto;
static OrientCandidate s_orient[8];
static uint32_t s_orient_packets = 0;
static uint32_t s_orient_window_packets = 0;
static uint32_t s_orient_window_slow_packets = 0;
static uint32_t s_orient_window_fast_packets = 0;
static uint32_t s_orient_pending_idx = 0;
static uint32_t s_orient_pending_windows = 0;

// QEMU-tuned defaults: react faster to sustained drift while still requiring
// confirmation across multiple windows to avoid flapping.
static constexpr uint32_t kOrientStartupPackets = 96;
static constexpr uint32_t kOrientWindowPackets = 96;
static constexpr uint32_t kOrientSwitchEdgeMargin = 6;
static constexpr uint32_t kDecodeStartupPackets = 64;
static constexpr uint32_t kDecodeWindowPackets = 96;
static constexpr uint32_t kDecodeSwitchEdgeMargin = 5;
static constexpr uint32_t kSpeedFastThreshold = 6;
static constexpr uint32_t kSpeedBucketMinPackets = 12;
static constexpr uint32_t kSwitchConfirmWindows = 2;

static inline int32_t iabs32(int32_t v) { return (v < 0) ? -v : v; }
static inline bool IsFastPacket(int dx, int dy) {
    return (uint32_t)(iabs32(dx) + iabs32(dy)) >= kSpeedFastThreshold;
}

static void ApplyOrient(MouseOrientMode mode, int inDx, int inDy, int& outDx, int& outDy) {
    switch (mode) {
        case MouseOrientMode::X_Y:   outDx =  inDx; outDy =  inDy; break;
        case MouseOrientMode::NX_Y:  outDx = -inDx; outDy =  inDy; break;
        case MouseOrientMode::X_NY:  outDx =  inDx; outDy = -inDy; break;
        case MouseOrientMode::NX_NY: outDx = -inDx; outDy = -inDy; break;
        case MouseOrientMode::Y_X:   outDx =  inDy; outDy =  inDx; break;
        case MouseOrientMode::NY_X:  outDx = -inDy; outDy =  inDx; break;
        case MouseOrientMode::Y_NX:  outDx =  inDy; outDy = -inDx; break;
        case MouseOrientMode::NY_NX: outDx = -inDy; outDy = -inDx; break;
        default:                     outDx =  inDx; outDy =  inDy; break;
    }
}

static const char* DecodeModeName(MouseDecodeMode mode) {
    switch (mode) {
        case MouseDecodeMode::HeaderSign: return "header-sign";
        case MouseDecodeMode::SignedByte: return "signed-byte";
        default: return "auto";
    }
}

static MouseDecodeMode DecodeIndexToMode(uint32_t idx) {
    return (idx == 0) ? MouseDecodeMode::HeaderSign : MouseDecodeMode::SignedByte;
}

static uint32_t DecodeModeToIndex(MouseDecodeMode mode) {
    return (mode == MouseDecodeMode::SignedByte) ? 1u : 0u;
}

static const char* OrientModeName(MouseOrientMode mode) {
    switch (mode) {
        case MouseOrientMode::X_Y: return "x_y";
        case MouseOrientMode::NX_Y: return "nx_y";
        case MouseOrientMode::X_NY: return "x_ny";
        case MouseOrientMode::NX_NY: return "nx_ny";
        case MouseOrientMode::Y_X: return "y_x";
        case MouseOrientMode::NY_X: return "ny_x";
        case MouseOrientMode::Y_NX: return "y_nx";
        case MouseOrientMode::NY_NX: return "ny_nx";
        default: return "auto";
    }
}

static uint32_t PickBestOrientationByTotals() {
    uint32_t best = 0;
    for (uint32_t i = 1; i < 8; ++i) {
        if (s_orient[i].edgeHits < s_orient[best].edgeHits) {
            best = i;
            continue;
        }
        if (s_orient[i].edgeHits == s_orient[best].edgeHits && s_orient[i].travel > s_orient[best].travel) {
            best = i;
        }
    }
    return best;
}

static uint32_t PickBestOrientationByWindow() {
    uint32_t best = 0;
    for (uint32_t i = 1; i < 8; ++i) {
        if (s_orient[i].edgeHitsWindow < s_orient[best].edgeHitsWindow) {
            best = i;
            continue;
        }
        if (s_orient[i].edgeHitsWindow == s_orient[best].edgeHitsWindow && s_orient[i].travelWindow > s_orient[best].travelWindow) {
            best = i;
        }
    }
    return best;
}

static uint32_t PickBestOrientationBySpeedWindow(bool fast) {
    uint32_t best = 0;
    for (uint32_t i = 1; i < 8; ++i) {
        uint32_t eBest = fast ? s_orient[best].edgeHitsWindowFast : s_orient[best].edgeHitsWindowSlow;
        uint32_t eCur = fast ? s_orient[i].edgeHitsWindowFast : s_orient[i].edgeHitsWindowSlow;
        if (eCur < eBest) {
            best = i;
            continue;
        }
        if (eCur == eBest) {
            uint32_t tBest = fast ? s_orient[best].travelWindowFast : s_orient[best].travelWindowSlow;
            uint32_t tCur = fast ? s_orient[i].travelWindowFast : s_orient[i].travelWindowSlow;
            if (tCur > tBest) best = i;
        }
    }
    return best;
}

static bool OrientationBucketClearlyBetter(uint32_t candidate, uint32_t current, bool fast, uint32_t margin) {
    uint32_t eCand = fast ? s_orient[candidate].edgeHitsWindowFast : s_orient[candidate].edgeHitsWindowSlow;
    uint32_t eCur = fast ? s_orient[current].edgeHitsWindowFast : s_orient[current].edgeHitsWindowSlow;
    return (eCand + margin < eCur);
}

static void SeedOrientationCandidatesToCursor() {
    int32_t x = 512;
    int32_t y = 384;
    uint8_t buttons = 0;
    if (kos::gfx::IsAvailable()) {
        const auto& fb = kos::gfx::GetInfo();
        x = (int32_t)(fb.width / 2);
        y = (int32_t)(fb.height / 2);
    }
    kos::ui::GetMouseState((int&)x, (int&)y, buttons);
    for (int i = 0; i < 8; ++i) {
        s_orient[i].x = x;
        s_orient[i].y = y;
    }
}

static void SeedDecodeCandidatesToCursor() {
    int32_t x = 512;
    int32_t y = 384;
    uint8_t buttons = 0;
    if (kos::gfx::IsAvailable()) {
        const auto& fb = kos::gfx::GetInfo();
        x = (int32_t)(fb.width / 2);
        y = (int32_t)(fb.height / 2);
    }
    kos::ui::GetMouseState((int&)x, (int&)y, buttons);
    for (int i = 0; i < 2; ++i) {
        s_decode[i].x = x;
        s_decode[i].y = y;
    }
}

static void ResetOrientationWindowStats() {
    s_orient_window_packets = 0;
    s_orient_window_slow_packets = 0;
    s_orient_window_fast_packets = 0;
    for (int i = 0; i < 8; ++i) {
        s_orient[i].edgeHitsWindow = 0;
        s_orient[i].travelWindow = 0;
        s_orient[i].edgeHitsWindowSlow = 0;
        s_orient[i].travelWindowSlow = 0;
        s_orient[i].edgeHitsWindowFast = 0;
        s_orient[i].travelWindowFast = 0;
    }
    SeedOrientationCandidatesToCursor();
}

static void ResetDecodeWindowStats() {
    s_decode_window_packets = 0;
    s_decode_window_slow_packets = 0;
    s_decode_window_fast_packets = 0;
    for (int i = 0; i < 2; ++i) {
        s_decode[i].edgeHitsWindow = 0;
        s_decode[i].travelWindow = 0;
        s_decode[i].edgeHitsWindowSlow = 0;
        s_decode[i].travelWindowSlow = 0;
        s_decode[i].edgeHitsWindowFast = 0;
        s_decode[i].travelWindowFast = 0;
    }
    SeedDecodeCandidatesToCursor();
}

static void ResetOrientationCalibration() {
    s_orient_mode = MouseOrientMode::Auto;
    s_orient_packets = 0;
    SeedOrientationCandidatesToCursor();
    for (int i = 0; i < 8; ++i) {
        s_orient[i].edgeHits = 0;
        s_orient[i].travel = 0;
        s_orient[i].edgeHitsWindow = 0;
        s_orient[i].travelWindow = 0;
        s_orient[i].edgeHitsWindowSlow = 0;
        s_orient[i].travelWindowSlow = 0;
        s_orient[i].edgeHitsWindowFast = 0;
        s_orient[i].travelWindowFast = 0;
    }
    s_orient_window_packets = 0;
    s_orient_window_slow_packets = 0;
    s_orient_window_fast_packets = 0;
    s_orient_pending_idx = 0;
    s_orient_pending_windows = 0;
}

static void ResetDecodeCalibration() {
    s_decode_mode = MouseDecodeMode::Auto;
    s_decode_packets = 0;
    SeedDecodeCandidatesToCursor();
    for (int i = 0; i < 2; ++i) {
        s_decode[i].edgeHits = 0;
        s_decode[i].travel = 0;
        s_decode[i].edgeHitsWindow = 0;
        s_decode[i].travelWindow = 0;
        s_decode[i].edgeHitsWindowSlow = 0;
        s_decode[i].travelWindowSlow = 0;
        s_decode[i].edgeHitsWindowFast = 0;
        s_decode[i].travelWindowFast = 0;
    }
    s_decode_window_packets = 0;
    s_decode_window_slow_packets = 0;
    s_decode_window_fast_packets = 0;
    s_decode_pending_idx = 0;
    s_decode_pending_windows = 0;
}

static void UpdateOrientationCalibration(int baseDx, int baseDy) {
    int32_t maxX = 1023;
    int32_t maxY = 767;
    if (kos::gfx::IsAvailable()) {
        const auto& fb = kos::gfx::GetInfo();
        maxX = (int32_t)(fb.width > 0 ? fb.width - 1 : 0);
        maxY = (int32_t)(fb.height > 0 ? fb.height - 1 : 0);
    }

    bool fastPacket = IsFastPacket(baseDx, baseDy);
    for (int i = 0; i < 8; ++i) {
        int tdx = 0, tdy = 0;
        ApplyOrient((MouseOrientMode)(i + 1), baseDx, baseDy, tdx, tdy);
        int32_t nx = s_orient[i].x + tdx;
        int32_t ny = s_orient[i].y + tdy;
        bool clamped = false;
        if (nx < 0) { nx = 0; clamped = true; }
        if (ny < 0) { ny = 0; clamped = true; }
        if (nx > maxX) { nx = maxX; clamped = true; }
        if (ny > maxY) { ny = maxY; clamped = true; }
        s_orient[i].x = nx;
        s_orient[i].y = ny;
        if (clamped) ++s_orient[i].edgeHits;
        s_orient[i].travel += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        if (clamped) ++s_orient[i].edgeHitsWindow;
        s_orient[i].travelWindow += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        if (fastPacket) {
            if (clamped) ++s_orient[i].edgeHitsWindowFast;
            s_orient[i].travelWindowFast += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        } else {
            if (clamped) ++s_orient[i].edgeHitsWindowSlow;
            s_orient[i].travelWindowSlow += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        }
    }

    ++s_orient_window_packets;
    if (fastPacket) ++s_orient_window_fast_packets; else ++s_orient_window_slow_packets;

    if (s_orient_mode == MouseOrientMode::Auto) {
        ++s_orient_packets;
        if (s_orient_packets < kOrientStartupPackets) return;

        uint32_t best = PickBestOrientationByTotals();
        s_orient_mode = (MouseOrientMode)(best + 1);
        Logger::Log((const int8_t*)"MOUSE: orientation calibrated");
        Logger::Log((const int8_t*)OrientModeName(s_orient_mode));
        s_orient_pending_idx = 0;
        s_orient_pending_windows = 0;
        ResetOrientationWindowStats();
        return;
    }

    if (s_orient_window_packets < kOrientWindowPackets) return;

    uint32_t current = (uint32_t)s_orient_mode - 1u;
    bool validSlow = s_orient_window_slow_packets >= kSpeedBucketMinPackets;
    bool validFast = s_orient_window_fast_packets >= kSpeedBucketMinPackets;
    uint32_t candidate = current;
    bool hasCandidate = false;
    if (validSlow && validFast) {
        uint32_t bestSlow = PickBestOrientationBySpeedWindow(false);
        uint32_t bestFast = PickBestOrientationBySpeedWindow(true);
        if (bestSlow == bestFast && bestSlow != current
            && OrientationBucketClearlyBetter(bestSlow, current, false, kOrientSwitchEdgeMargin)
            && OrientationBucketClearlyBetter(bestFast, current, true, kOrientSwitchEdgeMargin)) {
            candidate = bestSlow;
            hasCandidate = true;
        }
    } else if (validSlow) {
        uint32_t bestSlow = PickBestOrientationBySpeedWindow(false);
        if (bestSlow != current && OrientationBucketClearlyBetter(bestSlow, current, false, kOrientSwitchEdgeMargin)) {
            candidate = bestSlow;
            hasCandidate = true;
        }
    } else if (validFast) {
        uint32_t bestFast = PickBestOrientationBySpeedWindow(true);
        if (bestFast != current && OrientationBucketClearlyBetter(bestFast, current, true, kOrientSwitchEdgeMargin)) {
            candidate = bestFast;
            hasCandidate = true;
        }
    } else {
        uint32_t best = PickBestOrientationByWindow();
        if (best != current && (s_orient[best].edgeHitsWindow + kOrientSwitchEdgeMargin < s_orient[current].edgeHitsWindow)) {
            candidate = best;
            hasCandidate = true;
        }
    }

    if (hasCandidate) {
        if (s_orient_pending_idx == candidate + 1u) {
            ++s_orient_pending_windows;
        } else {
            s_orient_pending_idx = candidate + 1u;
            s_orient_pending_windows = 1;
        }
        if (s_orient_pending_windows >= kSwitchConfirmWindows) {
            s_orient_mode = (MouseOrientMode)(candidate + 1);
            s_orient_pending_idx = 0;
            s_orient_pending_windows = 0;
            Logger::Log((const int8_t*)"MOUSE: orientation recalibrated");
            Logger::Log((const int8_t*)OrientModeName(s_orient_mode));
        }
    } else {
        s_orient_pending_idx = 0;
        s_orient_pending_windows = 0;
    }
    ResetOrientationWindowStats();
}

static void ApplyFinalOrientation(int baseDx, int baseDy, int& outDx, int& outDy) {
    if (s_orient_mode == MouseOrientMode::Auto) {
        outDx = baseDx;
        outDy = baseDy;
        return;
    }
    ApplyOrient(s_orient_mode, baseDx, baseDy, outDx, outDy);
}

static void DecodeDeltasForMode(MouseDecodeMode mode, uint8_t b0, uint8_t b1, uint8_t b2, int& dx, int& dy) {
    if (mode == MouseDecodeMode::SignedByte) {
        dx = (int)(int8_t)b1;
        dy = (int)(int8_t)b2;
        return;
    }
    // PS/2 9-bit signed deltas using sign bits from header (bit4=X, bit5=Y)
    dx = (int)b1 - ((b0 & 0x10) ? 256 : 0);
    dy = (int)b2 - ((b0 & 0x20) ? 256 : 0);
}

static void UpdateDecodeCalibration(uint8_t b0, uint8_t b1, uint8_t b2) {
    int32_t maxX = 1023;
    int32_t maxY = 767;
    if (kos::gfx::IsAvailable()) {
        const auto& fb = kos::gfx::GetInfo();
        maxX = (int32_t)(fb.width > 0 ? fb.width - 1 : 0);
        maxY = (int32_t)(fb.height > 0 ? fb.height - 1 : 0);
    }

    int speedDx = 0, speedDy = 0;
    DecodeDeltasForMode(MouseDecodeMode::HeaderSign, b0, b1, b2, speedDx, speedDy);
    bool fastPacket = IsFastPacket(speedDx, -speedDy);

    for (uint32_t i = 0; i < 2; ++i) {
        int dx = 0, dy = 0;
        DecodeDeltasForMode(DecodeIndexToMode(i), b0, b1, b2, dx, dy);
        int tdx = dx;
        int tdy = -dy;
        int32_t nx = s_decode[i].x + tdx;
        int32_t ny = s_decode[i].y + tdy;
        bool clamped = false;
        if (nx < 0) { nx = 0; clamped = true; }
        if (ny < 0) { ny = 0; clamped = true; }
        if (nx > maxX) { nx = maxX; clamped = true; }
        if (ny > maxY) { ny = maxY; clamped = true; }
        s_decode[i].x = nx;
        s_decode[i].y = ny;
        if (clamped) ++s_decode[i].edgeHits;
        s_decode[i].travel += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        if (clamped) ++s_decode[i].edgeHitsWindow;
        s_decode[i].travelWindow += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        if (fastPacket) {
            if (clamped) ++s_decode[i].edgeHitsWindowFast;
            s_decode[i].travelWindowFast += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        } else {
            if (clamped) ++s_decode[i].edgeHitsWindowSlow;
            s_decode[i].travelWindowSlow += (uint32_t)(iabs32(tdx) + iabs32(tdy));
        }
    }

    ++s_decode_window_packets;
    if (fastPacket) ++s_decode_window_fast_packets; else ++s_decode_window_slow_packets;

    if (s_decode_mode == MouseDecodeMode::Auto) {
        ++s_decode_packets;
        if (s_decode_packets < kDecodeStartupPackets) return;
        uint32_t best = 0;
        if (s_decode[1].edgeHits < s_decode[0].edgeHits ||
            (s_decode[1].edgeHits == s_decode[0].edgeHits && s_decode[1].travel > s_decode[0].travel)) {
            best = 1;
        }
        s_decode_mode = DecodeIndexToMode(best);
        Logger::Log((const int8_t*)"MOUSE: decode calibrated");
        Logger::Log((const int8_t*)DecodeModeName(s_decode_mode));
        s_decode_pending_idx = 0;
        s_decode_pending_windows = 0;
        ResetDecodeWindowStats();
        return;
    }

    if (s_decode_window_packets < kDecodeWindowPackets) return;

    uint32_t current = DecodeModeToIndex(s_decode_mode);
    uint32_t other = (current == 0u) ? 1u : 0u;
    bool validSlow = s_decode_window_slow_packets >= kSpeedBucketMinPackets;
    bool validFast = s_decode_window_fast_packets >= kSpeedBucketMinPackets;
    bool betterSlow = s_decode[other].edgeHitsWindowSlow + kDecodeSwitchEdgeMargin < s_decode[current].edgeHitsWindowSlow;
    bool betterFast = s_decode[other].edgeHitsWindowFast + kDecodeSwitchEdgeMargin < s_decode[current].edgeHitsWindowFast;

    bool shouldSwitch = false;
    if (validSlow && validFast) {
        shouldSwitch = betterSlow && betterFast;
    } else if (validSlow) {
        shouldSwitch = betterSlow;
    } else if (validFast) {
        shouldSwitch = betterFast;
    } else {
        shouldSwitch = s_decode[other].edgeHitsWindow + kDecodeSwitchEdgeMargin < s_decode[current].edgeHitsWindow;
    }

    if (shouldSwitch) {
        if (s_decode_pending_idx == other + 1u) {
            ++s_decode_pending_windows;
        } else {
            s_decode_pending_idx = other + 1u;
            s_decode_pending_windows = 1;
        }
        if (s_decode_pending_windows >= kSwitchConfirmWindows) {
            s_decode_mode = DecodeIndexToMode(other);
            s_decode_pending_idx = 0;
            s_decode_pending_windows = 0;
            Logger::Log((const int8_t*)"MOUSE: decode recalibrated");
            Logger::Log((const int8_t*)DecodeModeName(s_decode_mode));
        }
    } else {
        s_decode_pending_idx = 0;
        s_decode_pending_windows = 0;
    }
    ResetDecodeWindowStats();
}

static void DecodeDeltasSelected(uint8_t b0, uint8_t b1, uint8_t b2, int& dx, int& dy) {
    MouseDecodeMode mode = s_decode_mode;
    if (mode == MouseDecodeMode::Auto) mode = MouseDecodeMode::HeaderSign;
    DecodeDeltasForMode(mode, b0, b1, b2, dx, dy);
}
}


MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
: InterruptHandler(manager, MOUSE_IRQ_VECTOR),
dataport(MOUSE_DATA_PORT),
commandport(MOUSE_COMMAND_PORT)
{
    this->handler = handler;
    // PS/2 controller already initialized in InitDrivers
}

MouseDriver::~MouseDriver()
{
}

void MouseDriver::Activate()
{
    offset = 0;
    poff = 0;
    buttons = 0;
    ResetDecodeCalibration();
    ResetOrientationCalibration();
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    const char* hx = "0123456789ABCDEF";

#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] Activate() start\n");
#endif

    // Flush pending output
    while (ps2.ReadStatus() & MOUSE_STATUS_OUTPUT_BUFFER) { (void)ps2.ReadData(); }

    // Enable auxiliary device
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_ENABLE_AUX);
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] 0xA8 sent\n");
#endif

    // Read config byte
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_READ_BYTE);
    ps2.WaitRead(); uint8_t cfgByte = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] cfg=0x");
    kos::lib::serial_putc(hx[(cfgByte >> 4) & 0xF]);
    kos::lib::serial_putc(hx[cfgByte & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Enable mouse IRQ (bit1) and ensure mouse clock enabled (clear disable bit5)
    cfgByte |= MOUSE_ENABLE_IRQ12_BIT;
    cfgByte &= ~(uint8_t)MOUSE_DISABLE_PORT2_BIT;

    // Write updated controller command byte back
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_WRITE_BYTE);
    ps2.WaitWrite(); ps2.WriteData(cfgByte);
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] new cfg=0x");
    kos::lib::serial_putc(hx[(cfgByte >> 4) & 0xF]);
    kos::lib::serial_putc(hx[cfgByte & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Small delay after config write
    for (volatile int i = 0; i < 10000; ++i) {}

    // Send Set Defaults (0xF6)
    ps2.WriteToMouse(MOUSE_CMD_SET_DEFAULTS);
    for (volatile int i = 0; i < 10000; ++i) {}
    ps2.WaitRead();
    uint8_t ack1 = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] F6 ack=0x");
    kos::lib::serial_putc(hx[(ack1 >> 4) & 0xF]);
    kos::lib::serial_putc(hx[ack1 & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Enable Data Reporting (0xF4)
    ps2.WriteToMouse(MOUSE_CMD_ENABLE_DATA_REPORTING);
    for (volatile int i = 0; i < 10000; ++i) {}
    ps2.WaitRead();
    uint8_t ack2 = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] F4 ack=0x");
    kos::lib::serial_putc(hx[(ack2 >> 4) & 0xF]);
    kos::lib::serial_putc(hx[ack2 & 0xF]);
    kos::lib::serial_write("\n");
#endif

#if KOS_INPUT_DEBUG
    if (ack1 == 0xFA && ack2 == 0xFA) {
        kos::lib::serial_write("[MOUSE] Activation OK\n");
    } else {
        kos::lib::serial_write("[MOUSE] Activation WARN - unexpected ACKs\n");
    }
#endif

    // Notify higher-level handler
    if (handler) handler->OnActivate();
}

uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
{
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0)
        return esp;
    if ((status & MOUSE_STATUS_AUX) == 0)
        return esp;

    uint8_t b = ps2.ReadData();

    // ACK/RESEND/SELF_TEST are control bytes, not packet payload.
    if (b == 0xFA || b == 0xFE || b == 0xAA) {
        offset = 0;
        return esp;
    }

    // Log first few non-control IRQ12 bytes to serial.
    static int irq_count = 0;
#if KOS_INPUT_DEBUG
    if (irq_count < 5) {
        const char* hx2 = "0123456789ABCDEF";
        kos::lib::serial_write("[MOUSE-IRQ] b=0x");
        kos::lib::serial_putc(hx2[(b >> 4) & 0xF]);
        kos::lib::serial_putc(hx2[b & 0xF]);
        kos::lib::serial_write("\n");
        ++irq_count;
    }
#endif

    // If we are waiting for the first byte of a packet, require sync bit set.
    if (offset == 0 && (b & MOUSE_SYNC_BIT) == 0) {
        return esp;
    }

    buffer[offset] = b;
    if (dumpEnabled && dumpCount < 96) {
        const char* hex = "0123456789ABCDEF";
        char msg[16]; int i = 0;
        msg[i++]='['; msg[i++]='M'; msg[i++]='D'; msg[i++]=']'; msg[i++]=' '; msg[i++]='I'; msg[i++]='R'; msg[i++]='Q'; msg[i++]=' '; msg[i++]='b'; msg[i++]='='; msg[i++]='0'; msg[i++]='x';
        msg[i++] = hex[(b>>4)&0xF]; msg[i++] = hex[b&0xF]; msg[i]=0;
        Logger::Log(msg);
        ++dumpCount;
        if (dumpCount == 96) { Logger::Log("[MD] dump complete"); dumpEnabled = false; }
    }

    if (handler == 0)
        return esp;

    offset = (offset + 1) % 3;
    if (offset != 0)
        return esp;

    // Ensure packet is aligned: bit3 in first byte must be 1.
    if ((buffer[0] & MOUSE_SYNC_BIT) == 0) {
        offset = 0;
        return esp;
    }

    // Discard packets with X/Y overflow (bits 6 and 7)
    if (buffer[0] & 0xC0) {
        return esp;
    }

    UpdateDecodeCalibration(buffer[0], buffer[1], buffer[2]);
    int dx = 0, dy = 0;
    DecodeDeltasSelected(buffer[0], buffer[1], buffer[2], dx, dy);
    int baseDx = dx;
    int baseDy = -dy;
    UpdateOrientationCalibration(baseDx, baseDy);
    int outDx = 0, outDy = 0;
    ApplyFinalOrientation(baseDx, baseDy, outDx, outDy);
    if (outDx != 0 || outDy != 0) {
        handler->OnMouseMove(outDx, outDy);
        enqueue_mouse_event(kos::input::EventType::MouseMove);
    }

    for (uint8_t i = 0; i < MOUSE_PACKET_SIZE; i++) {
        uint8_t mask = (1u << i);
        bool wasDown = (buttons & mask) != 0;
        bool isDown  = (buffer[0] & mask) != 0;
        if (wasDown != isDown) {
            if (isDown) {
                handler->OnMouseDown(i+1);
                enqueue_mouse_event(kos::input::EventType::MousePress);
            } else {
                handler->OnMouseUp(i+1);
                enqueue_mouse_event(kos::input::EventType::MouseRelease);
            }
        }
    }
    buttons = buffer[0];

    ::kos::drivers::mouse::g_mouse_packets++;
    ::kos::g_mouse_input_source = 1;

    static uint32_t pkt = 0; ++pkt;
    if (pkt == 1) {
        Logger::LogKV("MOUSE", "first-packet");
    } else if ((pkt & 63u) == 0 && Logger::IsDebugEnabled()) {
        Logger::Log("[MOUSE] pkt");
    }

    return esp;
}

// Fallback polling logic when IRQ12 is not firing.
// Reads one byte if available and assembles a packet; on full packet dispatches events.
void MouseDriver::PollOnce() {
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0) return; // nothing

    // Only read mouse data (AUX bit set).
    if ((status & MOUSE_STATUS_AUX) == 0) return;

    uint8_t b = ps2.ReadData();

    // Ignore controller/device response bytes in poll mode as well.
    if (b == 0xFA || b == 0xFE || b == 0xAA) {
        poff = 0;
        return;
    }

    static bool s_mouse_poll_tty = false;
    // If we are waiting for the first byte of a packet, require sync bit set.
    if (poff == 0 && (b & MOUSE_SYNC_BIT) == 0) {
        return;
    }

    pbuf[poff] = b;
    poff = (poff + 1) % 3;
    if (dumpEnabled && dumpCount < 96) {
        const char* hex = "0123456789ABCDEF";
        char msg[16]; int i=0;
        msg[i++]='['; msg[i++]='M'; msg[i++]='D'; msg[i++]=']'; msg[i++]=' '; msg[i++]='P'; msg[i++]='O'; msg[i++]='L'; msg[i++]=' '; msg[i++]='b'; msg[i++]='='; msg[i++]='0'; msg[i++]='x';
        msg[i++] = hex[(b>>4)&0xF]; msg[i++] = hex[b&0xF]; msg[i]=0;
        Logger::Log(msg);
        ++dumpCount;
        if (dumpCount == 96) { Logger::Log("[MD] dump complete"); dumpEnabled = false; }
    }
    if (poff != 0) return; // need full packet
    // Sync bit check
    if ((pbuf[0] & MOUSE_SYNC_BIT) == 0) { poff = 0; return; }
    // Discard packets with X/Y overflow (bits 6 and 7)
    if (pbuf[0] & 0xC0) { poff = 0; return; }
    UpdateDecodeCalibration(pbuf[0], pbuf[1], pbuf[2]);
    int dx = 0, dy = 0;
    DecodeDeltasSelected(pbuf[0], pbuf[1], pbuf[2], dx, dy);
    int baseDx = dx;
    int baseDy = -dy;
    UpdateOrientationCalibration(baseDx, baseDy);
    int outDx = 0, outDy = 0;
    ApplyFinalOrientation(baseDx, baseDy, outDx, outDy);
    if (handler && (outDx != 0 || outDy != 0)) {
        handler->OnMouseMove(outDx, outDy);
        enqueue_mouse_event(kos::input::EventType::MouseMove);
    }
    for (uint8_t i=0;i<3;++i) {
        uint8_t mask = (1u << i);
        bool wasDown = (buttons & mask) != 0;
        bool isDown  = (pbuf[0] & mask) != 0;
        if (wasDown != isDown && handler) {
            if (isDown) {
                handler->OnMouseDown(i+1);
                enqueue_mouse_event(kos::input::EventType::MousePress);
            } else {
                handler->OnMouseUp(i+1);
                enqueue_mouse_event(kos::input::EventType::MouseRelease);
            }
        }
    }
    buttons = pbuf[0];
    // Increment packet counter directly
    ::kos::drivers::mouse::g_mouse_packets++;
    ::kos::g_mouse_input_source = 2; // POLL
#if KOS_INPUT_DEBUG
    if (!s_mouse_poll_tty) { kos::console::TTY::Write((const int8_t*)"[MOUSE] using POLL\n"); s_mouse_poll_tty = true; }
#endif
    static uint32_t fp = 0; if (++fp == 1) Logger::LogKV("MOUSE", "first-packet-poll");
}