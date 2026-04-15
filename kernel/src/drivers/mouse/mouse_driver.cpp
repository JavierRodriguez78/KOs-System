//View https://wiki.osdev.org/Mouse_Input

#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/mouse/mouse_constants.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>
#include <kernel/input_debug.hpp>
#include <lib/serial.hpp>

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
    // PS/2 controller already initialized in InitDrivers
}

MouseDriver::~MouseDriver()
{
}

void MouseDriver::Activate()
{
    offset = 0;
    poff = 0;
    buttons = 0;
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    const char* hx = "0123456789ABCDEF";

#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] Activate() start\n");
#endif

    // Flush pending output
    while (ps2.ReadStatus() & MOUSE_STATUS_OUTPUT_BUFFER) { (void)ps2.ReadData(); }

    // Enable auxiliary device
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_ENABLE_AUX);
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] 0xA8 sent\n");
#endif

    // Read config byte
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_READ_BYTE);
    ps2.WaitRead(); uint8_t cfgByte = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] cfg=0x");
    kos::lib::serial_putc(hx[(cfgByte >> 4) & 0xF]);
    kos::lib::serial_putc(hx[cfgByte & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Enable mouse IRQ (bit1) and ensure mouse clock enabled (clear disable bit5)
    cfgByte |= MOUSE_ENABLE_IRQ12_BIT;
    cfgByte &= ~(uint8_t)MOUSE_DISABLE_PORT2_BIT;

    // Write updated controller command byte back
    ps2.WaitWrite(); ps2.WriteCommand(MOUSE_CMD_WRITE_BYTE);
    ps2.WaitWrite(); ps2.WriteData(cfgByte);
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] new cfg=0x");
    kos::lib::serial_putc(hx[(cfgByte >> 4) & 0xF]);
    kos::lib::serial_putc(hx[cfgByte & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Small delay after config write
    for (volatile int i = 0; i < 10000; ++i) {}

    // Send Set Defaults (0xF6)
    ps2.WriteToMouse(MOUSE_CMD_SET_DEFAULTS);
    for (volatile int i = 0; i < 10000; ++i) {}
    ps2.WaitRead();
    uint8_t ack1 = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] F6 ack=0x");
    kos::lib::serial_putc(hx[(ack1 >> 4) & 0xF]);
    kos::lib::serial_putc(hx[ack1 & 0xF]);
    kos::lib::serial_write("\n");
#endif

    // Enable Data Reporting (0xF4)
    ps2.WriteToMouse(MOUSE_CMD_ENABLE_DATA_REPORTING);
    for (volatile int i = 0; i < 10000; ++i) {}
    ps2.WaitRead();
    uint8_t ack2 = ps2.ReadData();
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[MOUSE] F4 ack=0x");
    kos::lib::serial_putc(hx[(ack2 >> 4) & 0xF]);
    kos::lib::serial_putc(hx[ack2 & 0xF]);
    kos::lib::serial_write("\n");
#endif

#if KOS_INPUT_DEBUG
    if (ack1 == 0xFA && ack2 == 0xFA) {
        kos::lib::serial_write("[MOUSE] Activation OK\n");
    } else {
        kos::lib::serial_write("[MOUSE] Activation WARN - unexpected ACKs\n");
    }
#endif

    // Notify higher-level handler
    if (handler) handler->OnActivate();
}

uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
{
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0)
        return esp;

    uint8_t b = ps2.ReadData();

    // ACK/RESEND/SELF_TEST are control bytes, not packet payload.
    if (b == 0xFA || b == 0xFE || b == 0xAA) {
        offset = 0;
        return esp;
    }

    // Log first few non-control IRQ12 bytes to serial.
    static int irq_count = 0;
#if KOS_INPUT_DEBUG
    if (irq_count < 5) {
        const char* hx2 = "0123456789ABCDEF";
        kos::lib::serial_write("[MOUSE-IRQ] b=0x");
        kos::lib::serial_putc(hx2[(b >> 4) & 0xF]);
        kos::lib::serial_putc(hx2[b & 0xF]);
        kos::lib::serial_write("\n");
        ++irq_count;
    }
#endif

    buffer[offset] = b;
    if (dumpEnabled && dumpCount < 96) {
        const char* hex = "0123456789ABCDEF";
        char msg[16]; int i = 0;
        msg[i++]='['; msg[i++]='M'; msg[i++]='D'; msg[i++]=']'; msg[i++]=' '; msg[i++]='I'; msg[i++]='R'; msg[i++]='Q'; msg[i++]=' '; msg[i++]='b'; msg[i++]='='; msg[i++]='0'; msg[i++]='x';
        msg[i++] = hex[(b>>4)&0xF]; msg[i++] = hex[b&0xF]; msg[i]=0;
        Logger::Log(msg);
        ++dumpCount;
        if (dumpCount == 96) { Logger::Log("[MD] dump complete"); dumpEnabled = false; }
    }

    if (handler == 0)
        return esp;

    offset = (offset + 1) % 3;
    if (offset != 0)
        return esp;

    // Ensure packet is aligned: bit3 in first byte must be 1.
    if ((buffer[0] & MOUSE_SYNC_BIT) == 0) {
        offset = 0;
        return esp;
    }

    int dx = (int8_t)buffer[1];
    int dy = (int8_t)buffer[2];
    if (dx != 0 || dy != 0)
        handler->OnMouseMove(dx, -dy);

    for (uint8_t i = 0; i < MOUSE_PACKET_SIZE; i++) {
        uint8_t mask = (1u << i);
        bool wasDown = (buttons & mask) != 0;
        bool isDown  = (buffer[0] & mask) != 0;
        if (wasDown != isDown) {
            if (isDown) handler->OnMouseDown(i+1);
            else        handler->OnMouseUp(i+1);
        }
    }
    buttons = buffer[0];

    ::kos::drivers::mouse::g_mouse_packets++;
    ::kos::g_mouse_input_source = 1;

    static uint32_t pkt = 0; ++pkt;
    if (pkt == 1) {
        Logger::LogKV("MOUSE", "first-packet");
    } else if ((pkt & 63u) == 0 && Logger::IsDebugEnabled()) {
        Logger::Log("[MOUSE] pkt");
    }

    return esp;
}

// Fallback polling logic when IRQ12 is not firing.
// Reads one byte if available and assembles a packet; on full packet dispatches events.
void MouseDriver::PollOnce() {
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0) return; // nothing

    // Only read mouse data (AUX bit set).
    if ((status & MOUSE_STATUS_AUX) == 0) return;

    uint8_t b = ps2.ReadData();

    // Ignore controller/device response bytes in poll mode as well.
    if (b == 0xFA || b == 0xFE || b == 0xAA) {
        poff = 0;
        return;
    }

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
#if KOS_INPUT_DEBUG
    if (!s_mouse_poll_tty) { kos::console::TTY::Write((const int8_t*)"[MOUSE] using POLL\n"); s_mouse_poll_tty = true; }
#endif
    static uint32_t fp = 0; if (++fp == 1) Logger::LogKV("MOUSE", "first-packet-poll");
}