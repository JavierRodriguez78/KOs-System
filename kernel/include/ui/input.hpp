#ifndef KOS_UI_INPUT_HPP
#define KOS_UI_INPUT_HPP

#include <common/types.hpp>
using namespace kos::common;

namespace kos { namespace ui {

// Initialize input subsystem (cursor position and button state)
void InitInput();

// Update mouse by relative delta and button state changes
void MouseMove(int dx, int dy);
void MouseButtonDown(uint8_t button);
void MouseButtonUp(uint8_t button);

// Retrieve current cursor x,y and buttons bitmask (bit0=left, bit1=right, bit2=middle)
void GetMouseState(int& x, int& y, uint8_t& buttons);

// Set cursor absolute position (clamped to screen)
void SetCursorPos(int x, int y);

// Set sensitivity scaling: applied as dx * num / den, dy * num / den (defaults 1/1)
void SetMouseSensitivity(int num, int den);

}}

#endif // KOS_UI_INPUT_HPP
