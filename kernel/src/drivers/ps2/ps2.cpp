#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>

using namespace kos::drivers::ps2;
using kos::console::Logger;

// Forward declare Port8Bit where it lives in the codebase
// Using existing Port8Bit from architecture I/O ports

static constexpr uint16_t PS2_DATA_PORT = 0x60;
static constexpr uint16_t PS2_CMD_PORT  = 0x64;

PS2Controller::PS2Controller()
    : m_data(nullptr), m_cmd(nullptr) {}

PS2Controller& PS2Controller::Instance() {
    static PS2Controller s_inst;
    return s_inst;
}

void PS2Controller::Init() {
    if (!m_data) m_data = new Port8Bit(PS2_DATA_PORT);
    if (!m_cmd)  m_cmd  = new Port8Bit(PS2_CMD_PORT);
}

uint8_t PS2Controller::ReadStatus() { return m_cmd->Read(); }
uint8_t PS2Controller::ReadData()   { return m_data->Read(); }
void    PS2Controller::WriteData(uint8_t v) { m_data->Write(v); }
void    PS2Controller::WriteCommand(uint8_t v) { m_cmd->Write(v); }

void    PS2Controller::WaitWrite() {
    for (int i = 0; i < 100000; ++i) { if ((ReadStatus() & PS2_STATUS_IBF) == 0) break; }
}
void    PS2Controller::WaitRead()  {
    for (int i = 0; i < 100000; ++i) { if (ReadStatus() & PS2_STATUS_OBF) break; }
}

void    PS2Controller::WriteToMouse(uint8_t v) {
    WaitWrite(); WriteCommand(PS2_CMD_WRITE_TO_MOUSE);
    WaitWrite(); WriteData(v);
}

uint8_t PS2Controller::ReadConfig() {
    WaitWrite(); WriteCommand(PS2_CMD_READ_CFG);
    WaitRead();  return ReadData();
}
