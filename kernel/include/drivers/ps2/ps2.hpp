#pragma once

// PS/2 controller abstraction for 8042 (ports 0x60/0x64)
#include <common/types.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>

namespace kos {
namespace drivers {
namespace ps2 {

using kos::common::uint8_t;
using kos::arch::x86::hardware::port::Port8Bit;

class PS2Controller {
public:
    static PS2Controller& Instance();

    void Init();

    uint8_t ReadStatus();
    uint8_t ReadData();
    void    WriteData(uint8_t value);
    void    WriteCommand(uint8_t value);
    void    WriteToMouse(uint8_t value);
    // Read current controller configuration byte (0x20).
    uint8_t ReadConfig();

    void    WaitWrite();
    void    WaitRead();

private:
    PS2Controller();
    PS2Controller(const PS2Controller&) = delete;
    PS2Controller& operator=(const PS2Controller&) = delete;

    Port8Bit* m_data;
    Port8Bit* m_cmd;
};

// PS/2 controller commands/constants
enum PS2Cmd : uint8_t {
    PS2_CMD_ENABLE_AUX = 0xA8,
    PS2_CMD_DISABLE_AUX = 0xA7,
    PS2_CMD_READ_CFG = 0x20,
    PS2_CMD_WRITE_CFG = 0x60,
    PS2_CMD_WRITE_TO_MOUSE = 0xD4,
};

// Status register bits
enum PS2Status : uint8_t {
    PS2_STATUS_OBF = 0x01,
    PS2_STATUS_IBF = 0x02,
    PS2_STATUS_AUX = 0x20,
};

// Mouse device commands
enum PS2MouseCmd : uint8_t {
    PS2_MOUSE_SET_DEFAULTS = 0xF6,
    PS2_MOUSE_ENABLE_DR = 0xF4,
};

} // namespace ps2
} // namespace drivers
} // namespace kos
