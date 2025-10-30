//View https://wiki.osdev.org/Mouse_Input

#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/mouse/mouse_constants.hpp>
#include <console/logger.hpp>

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

    // Enable auxiliary device
    wait_write(commandport);
    commandport.Write(MOUSE_CMD_ENABLE_AUX);

    // Read Controller Command Byte
    wait_write(commandport);
    commandport.Write(MOUSE_CMD_READ_BYTE);
    wait_read(commandport);
    uint8_t status = dataport.Read();

    // Enable mouse IRQ (bit1) and ensure mouse port is enabled (clear disable bit5)
    status |= MOUSE_ENABLE_IRQ12_BIT;
    status &= ~(uint8_t)MOUSE_DISABLE_PORT2_BIT;
    // Optionally ensure keyboard IRQ as well (bit0)
    // status |= 0x01;
    // Write Controller Command Byte back
    wait_write(commandport);
    commandport.Write(MOUSE_CMD_WRITE_BYTE);
    wait_write(commandport);
    dataport.Write(status);

    // Tell mouse: Set Defaults (0xF6), then Enable Data Reporting (0xF4)
    wait_write(commandport); commandport.Write(MOUSE_CMD_WRITE_TO_MOUSE);
    wait_write(commandport); dataport.Write(MOUSE_CMD_SET_DEFAULTS);
    wait_read(commandport); (void)dataport.Read(); // ACK

    wait_write(commandport); commandport.Write(MOUSE_CMD_WRITE_TO_MOUSE);
    wait_write(commandport); dataport.Write(MOUSE_CMD_ENABLE_DATA_REPORTING);
    wait_read(commandport); (void)dataport.Read(); // ACK

    if (Logger::IsDebugEnabled())
        Logger::Log("[MOUSE] PS/2 auxiliary device enabled and data reporting on");
    }
    
    uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
    {
        // Check if output buffer has data (bit0). We don't require AUX flag (bit5)
        // because we're already servicing IRQ12, so the byte is for the mouse.
        uint8_t status = commandport.Read();
        if ((status & MOUSE_STATUS_OUTPUT_BUFFER) == 0)
            return esp;

        buffer[offset] = dataport.Read();

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

            // Periodic debug to confirm movement/packets (every 64 packets)
            static uint32_t pkt = 0; ++pkt;
            if ((pkt & 63u) == 0 && Logger::IsDebugEnabled()) {
                Logger::Log("[MOUSE] pkt");
            }
        }
    return esp;
}