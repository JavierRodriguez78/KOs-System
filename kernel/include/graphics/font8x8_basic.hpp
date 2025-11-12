#ifndef KOS_GRAPHICS_FONT8X8_BASIC_HPP
#define KOS_GRAPHICS_FONT8X8_BASIC_HPP

#include <common/types.hpp>
using namespace kos::common;

// Minimal 8x8 bitmap font for ASCII 32..127 (public domain style)
// Each glyph is 8 bytes, one byte per row, MSB=left pixel.
namespace kos { namespace gfx {

extern const uint8_t kFont8x8Basic[96][8];

}}

#endif // KOS_GRAPHICS_FONT8X8_BASIC_HPP
