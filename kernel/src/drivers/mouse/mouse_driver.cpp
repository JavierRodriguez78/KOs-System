//View https://wiki.osdev.org/Mouse_Input

#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/mouse/mouse_constants.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>

using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers;
using namespace kos::drivers::mouse;


MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
: InterruptHandler(manager, MOUSE_IRQ_VECTOR),
dataport(MOUSE_DATA_PORT),
commandport(MOUSE_COMMAND_PORT)
{
    this->handler = handler;
    kos::drivers::ps2::PS2Controller::Instance().Init();
}

MouseDriver::~MouseDriver()
{
}
    
static inline void wait_write(Port8Bit& cmd) {
    // Wait until input buffer is clear (bit1 == 0)
    for (int i = 0; i < MOUSE_WAIT_MAX_ITERATIONS; ++i) { if ((cmd.Read() & MOUSE_STATUS_INPUT_BUFFER) == 0) break; }
}
    
static inline void wait_read(Port8Bit& cmd) {
    // Wait until output buffer is full (bit0 == 1)
    for (int i = 0; i < MOUSE_WAIT_MAX_ITERATIONS; ++i) { if (cmd.Read() & MOUSE_STATUS_OUTPUT_BUFFER) break; }
}

void MouseDriver::Activate()
{
    offset = 0;
    buttons = 0;
    // Use PS/2 controller abstraction
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    // Flush pending output
    while (ps2.ReadStatus() & MOUSE_STATUS_OUTPUT_BUFFER) { (void)ps2.ReadData(); }

    // Enable auxiliary device
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_ENABLE_AUX);

    // Read config byte
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_READ_BYTE);
    ps2.WaitRead();  uint8_t status = ps2.ReadData();

    uint8_t originalStatus = status;

    // Enable mouse IRQ (bit1) and ensure mouse clock enabled (clear disable bit5)
    status |= MOUSE_ENABLE_IRQ12_BIT;
    status &= ~(uint8_t)MOUSE_DISABLE_PORT2_BIT;

    // Write updated controller command byte back
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_WRITE_BYTE);
    ps2.WaitWrite(); ps2.WriteData(status);

    // Send Set Defaults (0xF6)
    ps2.WriteToMouse(MOUSE_CMD_SET_DEFAULTS);
    ps2.WaitRead(); uint8_t ack1 = ps2.ReadData();

    // Enable Data Reporting (0xF4)
    ps2.WriteToMouse(MOUSE_CMD_ENABLE_DATA_REPORTING);
    ps2.WaitRead(); uint8_t ack2 = ps2.ReadData();

    if (Logger::IsDebugEnabled()) {
        // Logger::Log only accepts a single message string; build simple hex output manually
        char msg[64];
        msg[0] = 0;
        // Show controller command byte change
        const char* hex = "0123456789ABCDEF";
        msg[0] = '['; msg[1] = 'M'; msg[2] = 'O'; msg[3] = 'U'; msg[4] = 'S'; msg[5] = 'E'; msg[6] = ']'; msg[7] = ' '; msg[8] = 'C'; msg[9] = 'f'; msg[10] = 'g'; msg[11] = ' '; msg[12] = 'o'; msg[13] = 'l'; msg[14] = 'd'; msg[15] = '=';
        msg[16] = '0'; msg[17] = 'x'; msg[18] = hex[(originalStatus >> 4) & 0xF]; msg[19] = hex[originalStatus & 0xF];
        msg[20] = ' '; msg[21] = 'n'; msg[22] = 'e'; msg[23] = 'w'; msg[24] = '='; msg[25] = '0'; msg[26] = 'x'; msg[27] = hex[(status >> 4) & 0xF]; msg[28] = hex[status & 0xF]; msg[29] = 0;
        Logger::Log(msg);
        char msg2[64];
        msg2[0] = '['; msg2[1] = 'M'; msg2[2] = 'O'; msg2[3] = 'U'; msg2[4] = 'S'; msg2[5] = 'E'; msg2[6] = ']'; msg2[7] = ' '; msg2[8] = 'A'; msg2[9] = 'c'; msg2[10] = 'k'; msg2[11] = 's'; msg2[12] = ':'; msg2[13] = ' '; msg2[14] = 'F'; msg2[15] = '6'; msg2[16] = '='; msg2[17] = '0'; msg2[18] = 'x'; msg2[19] = hex[(ack1 >> 4) & 0xF]; msg2[20] = hex[ack1 & 0xF];
        msg2[21] = ' '; msg2[22] = 'F'; msg2[23] = '4'; msg2[24] = '='; msg2[25] = '0'; msg2[26] = 'x'; msg2[27] = hex[(ack2 >> 4) & 0xF]; msg2[28] = hex[ack2 & 0xF]; msg2[29] = 0;
        Logger::Log(msg2);
    }

    // Notify higher-level handler (e.g., to set initial position or state)
    if (handler) handler->OnActivate();

    if (Logger::IsDebugEnabled())
        Logger::Log("[MOUSE] PS/2 auxiliary device enabled; data reporting active");
    }
    
    uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
    {
        // Check if output buffer has data (bit0). We don't require AUX flag (bit5)
        // because we're already servicing IRQ12, so the byte is for the mouse.
        auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
        uint8_t status = ps2.ReadStatus();
        if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0)
            return esp;

        uint8_t b = ps2.ReadData();
        buffer[offset] = b;
        if (dumpEnabled && dumpCount < 96) {
            // Print raw byte in hex, grouped
            const char* hex = "0123456789ABCDEF";
            char msg[16]; int i=0;
            msg[i++]='['; msg[i++]='M'; msg[i++]='D'; msg[i++]=']'; msg[i++]=' '; msg[i++]='I'; msg[i++]='R'; msg[i++]='Q'; msg[i++]=' '; msg[i++]='b'; msg[i++]='='; msg[i++]='0'; msg[i++]='x';
            msg[i++] = hex[(b>>4)&0xF]; msg[i++] = hex[b&0xF]; msg[i]=0;
            Logger::Log(msg);
            ++dumpCount;
            if (dumpCount == 96) { Logger::Log("[MD] dump complete"); dumpEnabled = false; }
        }

         if (handler == 0)
            return esp;

        offset = (offset + 1) % 3;

        if(offset == 0)
        {
            // Ensure packet is aligned: bit3 of first byte should always be 1
            if ((buffer[0] & MOUSE_SYNC_BIT) == 0) {
                // Desync; reset packet assembly
                offset = 0;
                return esp;
            }
            int dx = (int8_t)buffer[1];
            int dy = (int8_t)buffer[2];
            if(dx != 0 || dy != 0)
            {
                handler->OnMouseMove(dx, -dy);
            }

           for(uint8_t i = 0; i < MOUSE_PACKET_SIZE; i++)
            {
                uint8_t mask = (1u << i);
                bool wasDown = (buttons & mask) != 0;
                bool isDown  = (buffer[0] & mask) != 0;
                if (wasDown != isDown) {
                    if (isDown) handler->OnMouseDown(i+1);
                    else        handler->OnMouseUp(i+1);
                }
            }
            buttons = buffer[0];

            // Increment diagnostic counter for overlay
            ::kos::drivers::mouse::g_mouse_packets++;

            // Periodic debug to confirm movement/packets (every 64 packets)
            static uint32_t pkt = 0; ++pkt;
            if (pkt == 1) {
                Logger::LogKV("MOUSE", "first-packet");
                ::kos::g_mouse_input_source = 1; // IRQ
            } else if ((pkt & 63u) == 0 && Logger::IsDebugEnabled()) {
                Logger::Log("[MOUSE] pkt");
            }
        }
    return esp;
}

// Fallback polling logic when IRQ12 is not firing.
// Reads one byte if available and assembles a packet; on full packet dispatches events.
void MouseDriver::PollOnce() {
    // Check if data available
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0) return; // nothing
    // Do not require AUX bit: some emulators/hosts may not set it reliably
    uint8_t b = ps2.ReadData();
    static bool s_mouse_poll_tty = false;
    pbuf[poff] = b;
    poff = (poff + 1) % 3;
    if (dumpEnabled && dumpCount < 96) {
        const char* hex = "0123456789ABCDEF";
        char msg[16]; int i=0;
        msg[i++]='['; msg[i++]='M'; msg[i++]='D'; msg[i++]=']'; msg[i++]=' '; msg[i++]='P'; msg[i++]='O'; msg[i++]='L'; msg[i++]=' '; msg[i++]='b'; msg[i++]='='; msg[i++]='0'; msg[i++]='x';
        msg[i++] = hex[(b>>4)&0xF]; msg[i++] = hex[b&0xF]; msg[i]=0;
        Logger::Log(msg);
        ++dumpCount;
        if (dumpCount == 96) { Logger::Log("[MD] dump complete"); dumpEnabled = false; }
    }
    if (poff != 0) return; // need full packet
    // Sync bit check
    if ((pbuf[0] & MOUSE_SYNC_BIT) == 0) { poff = 0; return; }
    int dx = (int8_t)pbuf[1];
    int dy = (int8_t)pbuf[2];
    if (handler && (dx != 0 || dy != 0)) handler->OnMouseMove(dx, -dy);
    for (uint8_t i=0;i<3;++i) {
        uint8_t mask = (1u << i);
        bool wasDown = (buttons & mask) != 0;
        bool isDown  = (pbuf[0] & mask) != 0;
        if (wasDown != isDown && handler) {
            if (isDown) handler->OnMouseDown(i+1); else handler->OnMouseUp(i+1);
        }
    }
    buttons = pbuf[0];
    // Increment packet counter directly
    ::kos::drivers::mouse::g_mouse_packets++;
    ::kos::g_mouse_input_source = 2; // POLL
    if (!s_mouse_poll_tty) { kos::console::TTY::Write((const int8_t*)"[MOUSE] using POLL\n"); s_mouse_poll_tty = true; }
    static uint32_t fp = 0; if (++fp == 1) Logger::LogKV("MOUSE", "first-packet-poll");
}