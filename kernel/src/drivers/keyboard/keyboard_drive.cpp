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

    // Handle extended scancode prefix 0xE0 (for extended keys like right Ctrl, keypad '/').
    if (key == 0xE0) {
        e0Prefix = true;
        return esp;
    }

    // Handle key release (break) codes to maintain modifier state
    if (key & 0x80) {
        uint8_t code = key & 0x7F;
        if (e0Prefix) {
            // Extended releases
            if (code == 0x1D) { // Right Ctrl release
                ctrlRight = false;
            }
            e0Prefix = false;
        } else {
            if (code == 0x1D) { // Left Ctrl release
                ctrlLeft = false;
            }
        }
        return esp;
    }

    // Make (key press) codes are < 0x80
    if(key < 0x80)
    {
        // If previous byte was 0xE0, handle extended mappings
        if (e0Prefix) {
            e0Prefix = false;
            switch (key) {
                case 0x1D: // Right Ctrl press
                    ctrlRight = true;
                    break;
                case 0x49: // Page Up (E0 49)
                    handler->OnKeyDown((char)0xF1); // internal code for PageUp
                    break;
                case 0x51: // Page Down (E0 51)
                    handler->OnKeyDown((char)0xF2); // internal code for PageDown
                    break;
                case 0x35: handler->OnKeyDown('/'); break; // Keypad '/' (E0 35)
                case 0x4A: handler->OnKeyDown('-'); break; // Keypad '-' (E0 4A) on some layouts
                default:
                    // Unhandled E0 make code; optional debug
                    // tty.Write("E0 "); tty.WriteHex(key);
                    break;
            }
            return esp;
        }

        int8_t ch = 0;
        switch(key)
        {
            // Backspace
            case 0x0E: ch = '\b'; break; 
            // Numeric Keys (main row)
            case 0x02: ch = '1'; break;
            case 0x03: ch = '2'; break;
            case 0x04: ch = '3'; break;
            case 0x05: ch = '4'; break;
            case 0x06: ch = '5'; break;
            case 0x07: ch = '6'; break;
            case 0x08: ch = '7'; break;
            case 0x09: ch = '8'; break;
            case 0x0A: ch = '9'; break;
            case 0x0B: ch = '0'; break;
            case 0x0C: ch = '-'; break; // Main row '-'

            case 0x10: ch = 'q'; break;
            case 0x11: ch = 'w'; break;
            case 0x12: ch = 'e'; break;
            case 0x13: ch = 'r'; break;
            case 0x14: ch = 't'; break;
            case 0x15: ch = 'z'; break;
            case 0x16: ch = 'u'; break;
            case 0x17: ch = 'i'; break;
            case 0x18: ch = 'o'; break;
            case 0x19: ch = 'p'; break;

            case 0x1E: ch = 'a'; break;
            case 0x1F: ch = 's'; break;
            case 0x20: ch = 'd'; break;
            case 0x21: ch = 'f'; break;
            case 0x22: ch = 'g'; break;
            case 0x23: ch = 'h'; break;
            case 0x24: ch = 'j'; break;
            case 0x25: ch = 'k'; break;
            case 0x26: ch = 'l'; break;

            case 0x2C: ch = 'y'; break;
            case 0x2D: ch = 'x'; break;
            case 0x2E: ch = 'c'; break;
            case 0x2F: ch = 'v'; break;
            case 0x30: ch = 'b'; break;
            case 0x31: ch = 'n'; break;
            case 0x32: ch = 'm'; break;
            case 0x33: ch = ','; break;
            case 0x34: ch = '.'; break;
            case 0x35: ch = '-'; break; // Map to '-' per layout
            case 0x4A: ch = '-'; break; // Keypad '-'

            case 0x1C: ch = '\n'; break;
            case 0x39: ch = ' '; break;

            case 0x1D: // Left Ctrl press
                ctrlLeft = true;
                break;

            default:
            {
                tty.Write("KEYBOARD 0X");
                tty.WriteHex(key);
                break; // ch remains 0
            }
        }

        // If we obtained a character, apply Ctrl mapping if needed and deliver
        if (ch) {
            if ((ctrlLeft || ctrlRight)) {
                // Map letters to control codes 1..26
                if (ch >= 'a' && ch <= 'z') {
                    ch = (int8_t)(ch - 'a' + 1);
                } else if (ch >= 'A' && ch <= 'Z') {
                    ch = (int8_t)(ch - 'A' + 1);
                }
            }
            handler->OnKeyDown(ch);
        }
    }

    return esp;
};