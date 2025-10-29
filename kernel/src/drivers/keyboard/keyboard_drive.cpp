#include <drivers/keyboard/keyboard_driver.hpp>
using namespace kos::drivers::keyboard;


KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler)
:InterruptHandler(manager, 0x21),dataport(0x60),commandport(0x64){
    this->handler = handler;
};


KeyboardDriver::~KeyboardDriver()
{

};



void KeyboardDriver::Activate()
{
    static TTY tty;
    tty.Write("[KBD] Activating keyboard...\n");
    
    // Clear keyboard buffer
    while(commandport.Read() & 0x1) {
        dataport.Read();
    }
    
    // Wait for keyboard controller to be ready
    int timeout = 1000;
    while ((commandport.Read() & 0x02) && timeout-- > 0) {
        // Wait for input buffer to be empty
    }
    
    tty.Write("[KBD] Disabling devices...\n");
    commandport.Write(0xAD); // Disable first PS/2 port
    commandport.Write(0xA7); // Disable second PS/2 port (if exists)
    
    // Clear buffer again
    while(commandport.Read() & 0x1) {
        dataport.Read();
    }
    
    tty.Write("[KBD] Reading controller config...\n");
    commandport.Write(0x20); // Read controller config
    uint8_t config = dataport.Read();
    tty.Write("[KBD] Controller config: ");
    tty.WriteHex(config);
    tty.Write("\n");
    
    // Set configuration - enable interrupts and scanning
    config |= 0x01;  // Enable first PS/2 port interrupt
    config &= ~0x10; // Enable first PS/2 port
    config &= ~0x20; // Enable second PS/2 port (if exists)
    
    tty.Write("[KBD] Writing new config: ");
    tty.WriteHex(config);
    tty.Write("\n");
    commandport.Write(0x60); // Write controller config
    dataport.Write(config);
    
    tty.Write("[KBD] Enabling first PS/2 port...\n");
    commandport.Write(0xAE); // Enable first PS/2 port
    
    tty.Write("[KBD] Sending enable command to keyboard...\n");
    dataport.Write(0xF4); // Enable scanning
    
    // Wait for acknowledgment
    timeout = 1000;
    while (timeout-- > 0 && !(commandport.Read() & 0x01)) {
        // Wait for response
    }
    
    if (commandport.Read() & 0x01) {
        uint8_t response = dataport.Read();
        tty.Write("[KBD] Keyboard response: ");
        tty.WriteHex(response);
        tty.Write("\n");
    } else {
        tty.Write("[KBD] No keyboard response\n");
    }
    
    tty.Write("[KBD] Keyboard activation complete\n");
};
  
uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp)
{
    uint8_t key = dataport.Read();

    if(handler == 0)
        return esp;

    // Handle extended scancode prefix 0xE0 (for keys like keypad '/').
    if (key == 0xE0) {
        e0Prefix = true;
        return esp;
    }

    // Break (key release) codes are >= 0x80; we only process make codes here.
    if(key < 0x80)
    {
        // If previous byte was 0xE0, handle extended mappings
        if (e0Prefix) {
            e0Prefix = false;
            switch (key) {
                case 0x35: handler->OnKeyDown('/'); break; // Keypad '/' (E0 35)
                case 0x4A: handler->OnKeyDown('-'); break; // Keypad '-' (E0 4A) on some layouts
                default:
                    // Unhandled E0 make code; optional debug
                    // tty.Write("E0 "); tty.WriteHex(key);
                    break;
            }
            return esp;
        }

        switch(key)
            
        {
            // Backspace
            case 0x0E: handler->OnKeyDown('\b'); break; 
            // Numeric Keys (main row)
            case 0x02: handler->OnKeyDown('1'); break;
            case 0x03: handler->OnKeyDown('2'); break;
            case 0x04: handler->OnKeyDown('3'); break;
            case 0x05: handler->OnKeyDown('4'); break;
            case 0x06: handler->OnKeyDown('5'); break;
            case 0x07: handler->OnKeyDown('6'); break;
            case 0x08: handler->OnKeyDown('7'); break;
            case 0x09: handler->OnKeyDown('8'); break;
            case 0x0A: handler->OnKeyDown('9'); break;
            case 0x0B: handler->OnKeyDown('0'); break;
            case 0x0C: handler->OnKeyDown('-'); break; // Main row '-'

            case 0x10: handler->OnKeyDown('q'); break;
            case 0x11: handler->OnKeyDown('w'); break;
            case 0x12: handler->OnKeyDown('e'); break;
            case 0x13: handler->OnKeyDown('r'); break;
            case 0x14: handler->OnKeyDown('t'); break;
            case 0x15: handler->OnKeyDown('z'); break;
            case 0x16: handler->OnKeyDown('u'); break;
            case 0x17: handler->OnKeyDown('i'); break;
            case 0x18: handler->OnKeyDown('o'); break;
            case 0x19: handler->OnKeyDown('p'); break;

            case 0x1E: handler->OnKeyDown('a'); break;
            case 0x1F: handler->OnKeyDown('s'); break;
            case 0x20: handler->OnKeyDown('d'); break;
            case 0x21: handler->OnKeyDown('f'); break;
            case 0x22: handler->OnKeyDown('g'); break;
            case 0x23: handler->OnKeyDown('h'); break;
            case 0x24: handler->OnKeyDown('j'); break;
            case 0x25: handler->OnKeyDown('k'); break;
            case 0x26: handler->OnKeyDown('l'); break;

            case 0x2C: handler->OnKeyDown('y'); break;
            case 0x2D: handler->OnKeyDown('x'); break;
            case 0x2E: handler->OnKeyDown('c'); break;
            case 0x2F: handler->OnKeyDown('v'); break;
            case 0x30: handler->OnKeyDown('b'); break;
            case 0x31: handler->OnKeyDown('n'); break;
            case 0x32: handler->OnKeyDown('m'); break;
            case 0x33: handler->OnKeyDown(','); break;
            case 0x34: handler->OnKeyDown('.'); break;
            case 0x35: handler->OnKeyDown('-'); break; // Map to '-' per layout
            case 0x4A: handler->OnKeyDown('-'); break; // Keypad '-'

            case 0x1C: handler->OnKeyDown('\n'); break;
            case 0x39: handler->OnKeyDown(' '); break;

            default:
            {
                tty.Write("KEYBOARD 0X");
                tty.WriteHex(key);
                break;
            }
        }
    }

    return esp;
};