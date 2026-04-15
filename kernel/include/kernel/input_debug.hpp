#ifndef KOS_KERNEL_INPUT_DEBUG_HPP
#define KOS_KERNEL_INPUT_DEBUG_HPP

// Centralized input debug switch.
// 0: production (quiet)
// 1: verbose input traces (keyboard/mouse/IRQ diagnostics)
#ifndef KOS_INPUT_DEBUG
#define KOS_INPUT_DEBUG 0
#endif

#endif // KOS_KERNEL_INPUT_DEBUG_HPP
