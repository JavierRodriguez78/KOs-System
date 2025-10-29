#pragma once

#include <common/types.hpp>

using namespace kos::common;
namespace kos{
    namespace drivers
    {
        namespace vga
        {
            // VGA text mode constants
            constexpr uint16_t VGA_WIDTH = 80;
            constexpr uint16_t VGA_HEIGHT = 25;
            constexpr uint8_t VGA_COLOR_DEFAULT= 0X07; // light grey on black
            constexpr int8_t VGA_BLANK_CHAR = ' ';
            constexpr uintptr_t VGA_TEXT_BUFFER = 0xB8000;
        }
    }
}