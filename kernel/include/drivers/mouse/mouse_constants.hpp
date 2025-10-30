#pragma once

#include <common/types.hpp>
using namespace kos::common;

namespace kos::drivers::mouse {

    // IRQ & Ports
    constexpr uint8_t MOUSE_IRQ_VECTOR           = 0x2C;
    constexpr uint16_t MOUSE_DATA_PORT           = 0x60;
    constexpr uint16_t MOUSE_COMMAND_PORT        = 0x64;

    // Status bits
    constexpr uint8_t MOUSE_STATUS_OUTPUT_BUFFER = 0x01; // bit0
    constexpr uint8_t MOUSE_STATUS_INPUT_BUFFER  = 0x02; // bit1

    // Controller commands
    constexpr uint8_t MOUSE_CMD_ENABLE_AUX           = 0xA8;
    constexpr uint8_t MOUSE_CMD_READ_BYTE            = 0x20;
    constexpr uint8_t MOUSE_CMD_WRITE_BYTE           = 0x60;
    constexpr uint8_t MOUSE_CMD_WRITE_TO_MOUSE       = 0xD4;

    // Mouse device commands
    constexpr uint8_t MOUSE_CMD_SET_DEFAULTS         = 0xF6;
    constexpr uint8_t MOUSE_CMD_ENABLE_DATA_REPORTING= 0xF4;

    // Command byte flags
    constexpr uint8_t MOUSE_ENABLE_IRQ12_BIT         = 0x02;
    constexpr uint8_t MOUSE_DISABLE_PORT2_BIT        = 0x20;

    // Packet and data
    constexpr uint8_t MOUSE_PACKET_SIZE              = 3;
    constexpr uint8_t MOUSE_SYNC_BIT                 = 0x08;

    // Timing and diagnostics
    constexpr uint32_t MOUSE_WAIT_MAX_ITERATIONS     = 100000;
    constexpr uint32_t MOUSE_DEBUG_PACKET_INTERVAL   = 64;

} // namespace kos::drivers::mouse