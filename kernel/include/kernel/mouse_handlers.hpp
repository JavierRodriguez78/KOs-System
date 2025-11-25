#pragma once
#include <drivers/mouse/mouse_event_handler.hpp>

namespace kos { namespace kernel {
    // Ensure mouse handler instances used by drivers and services are
    // constructed before drivers register with the InterruptManager.
    void InitKernelMouseHandlers();
} }
