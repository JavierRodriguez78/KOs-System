#ifndef KOS_GRAPHICS_FONT8X16_BASIC_HPP
#define KOS_GRAPHICS_FONT8X16_BASIC_HPP

#include <common/types.hpp>
using kos::common::uint8_t;

// Minimal 8x16 font mapping ASCII 32..127. For brevity only a subset of glyphs are fully distinct; others reuse top/bottom pattern.
// Each glyph: 16 bytes, LSB-left.
namespace kos { namespace gfx {
extern const uint8_t kFont8x16Basic[96][16];
}}

#endif