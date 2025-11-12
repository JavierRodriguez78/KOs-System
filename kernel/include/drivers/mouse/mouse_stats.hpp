#pragma once

#include <common/types.hpp>
using namespace kos::common;

namespace kos { namespace drivers { namespace mouse {

// Global packet counter storage (defined in mouse_stats.cpp)
extern volatile uint32_t g_mouse_packets;

// Inline helpers to avoid linkage issues if object not yet added
inline void MousePkt_Increment() { ++g_mouse_packets; }
inline uint32_t MousePkt_Get() { return g_mouse_packets; }

}}}
