#ifndef KOS_UI_CURSOR_HPP
#define KOS_UI_CURSOR_HPP

#include <common/types.hpp>

namespace kos { namespace ui {

enum class CursorStyle : kos::common::uint8_t {
    Crosshair = 0,
    Triangle  = 1
};

// Set the active cursor style (affects next frame rendering)
void SetCursorStyle(CursorStyle s);
CursorStyle GetCursorStyle();

// Render the cursor using the compositor; expects mouse state already updated.
// Called late in the frame after all windows and terminal content so cursor stays on top.
void RenderCursor();

}}

#endif // KOS_UI_CURSOR_HPP
