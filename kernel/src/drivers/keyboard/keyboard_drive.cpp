#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>
#include <lib/serial.hpp>
using namespace kos::drivers::keyboard;
using namespace kos::console;


KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler)
:InterruptHandler(manager, 0x21),dataport(0x60),commandport(0x64){
    this->handler = handler;
    // PS/2 controller already initialized in InitDrivers
};


KeyboardDriver::~KeyboardDriver()
{

};



void KeyboardDriver::Activate()
{
    static TTY tty;
    tty.Write("[KBD] Activating keyboard...\n");
    
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    // Clear keyboard buffer
    while(ps2.ReadStatus() & 0x1) { (void)ps2.ReadData(); }
    
    // Wait for keyboard controller to be ready
    int timeout = 1000;
    while ((ps2.ReadStatus() & 0x02) && timeout-- > 0) {
        // Wait for input buffer to be empty
    }
    
    tty.Write("[KBD] Disabling devices...\n");
    ps2.WaitWrite(); ps2.WriteCommand(0xAD); // Disable first PS/2 port
    ps2.WaitWrite(); ps2.WriteCommand(0xA7); // Disable second PS/2 port (if exists)
    
    // Clear buffer again
    while(ps2.ReadStatus() & 0x1) { (void)ps2.ReadData(); }
    
    tty.Write("[KBD] Reading controller config...\n");
    ps2.WaitWrite(); ps2.WriteCommand(0x20); // Read controller config
    ps2.WaitRead();
    uint8_t config = ps2.ReadData();
    tty.Write("[KBD] Controller config: ");
    tty.WriteHex(config);
    tty.Write("\n");
    
    // Set configuration - enable interrupts and scanning for BOTH ports
    config |= 0x01;  // Enable first PS/2 port interrupt (keyboard)
    config |= 0x02;  // Enable second PS/2 port interrupt (mouse)
    config &= ~0x10; // Enable first PS/2 port clock
    config &= ~0x20; // Enable second PS/2 port clock
    
    tty.Write("[KBD] Writing new config: ");
    tty.WriteHex(config);
    tty.Write("\n");
    ps2.WaitWrite(); ps2.WriteCommand(0x60); // Write controller config
    ps2.WaitWrite(); ps2.WriteData(config);
    
    tty.Write("[KBD] Enabling first PS/2 port...\n");
    ps2.WaitWrite(); ps2.WriteCommand(0xAE); // Enable first PS/2 port
    
    tty.Write("[KBD] Sending enable command to keyboard...\n");
    // For keyboard on port 1, write device command directly to data port (0x60)
    ps2.WaitWrite(); ps2.WriteData(0xF4); // Enable scanning
    
    // Wait for acknowledgment
    timeout = 1000;
    while (timeout-- > 0 && !(ps2.ReadStatus() & 0x01)) {
        // Wait for response
    }
    
    if (ps2.ReadStatus() & 0x01) {
        uint8_t response = ps2.ReadData();
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
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t key = ps2.ReadData();

    // Mark that we received some keyboard activity, even if it's a modifier/release
    ::kos::g_kbd_input_source = 1;
    ++::kos::g_kbd_events;
    
    // Debug: log first few interrupts to serial
    static int irq_count = 0;
    if (irq_count < 5) {
        kos::lib::serial_write("[KBD-IRQ] scancode: 0x");
        const char* hex = "0123456789ABCDEF";
        kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
        kos::lib::serial_putc(hex[key & 0xF]);
        kos::lib::serial_write("\n");
        ++irq_count;
    }

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
                case 0x1C: // Numpad Enter (E0 1C)
                    handler->OnKeyDown('\n');
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
                    break;
                case 0x49: // Page Up (E0 49)
                    handler->OnKeyDown((char)0xF1); // internal code for PageUp
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
                    break;
                case 0x51: // Page Down (E0 51)
                    handler->OnKeyDown((char)0xF2); // internal code for PageDown
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
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
                // Suppress noisy scancode debug in normal builds; enable with -DKBD_DEBUG
                #ifdef KBD_DEBUG
                tty.Write("KEYBOARD 0X");
                tty.WriteHex(key);
                #endif
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
            
            // Debug: log first few characters delivered to handler
            static int char_count = 0;
            if (char_count < 5) {
                kos::lib::serial_write("[KBD-IRQ] delivering char: ");
                if (ch >= 32 && ch <= 126) {
                    kos::lib::serial_putc((char)ch);
                } else {
                    kos::lib::serial_write("0x");
                    const char* hex = "0123456789ABCDEF";
                    kos::lib::serial_putc(hex[((uint8_t)ch >> 4) & 0xF]);
                    kos::lib::serial_putc(hex[(uint8_t)ch & 0xF]);
                }
                kos::lib::serial_write("\n");
                ++char_count;
            }
            
            handler->OnKeyDown(ch);
            
            // Debug: verify handler isn't null
            static int handler_check = 0;
            if (handler_check++ < 3) {
                kos::lib::serial_write("[KBD] handler ptr=");
                const char* hex = "0123456789ABCDEF";
                uintptr_t addr = (uintptr_t)handler;
                for (int i = 7; i >= 0; --i) {
                    kos::lib::serial_putc(hex[(addr >> (i*4)) & 0xF]);
                }
                kos::lib::serial_write("\n");
            }
            
            // mark source as IRQ
            ::kos::g_kbd_input_source = 1;
            static bool s_first_irq_logged = false;
            if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
        }
    }

    return esp;
};

// Fallback: poll controller status and process one scancode identically to interrupt path.
bool KeyboardDriver::PollOnce() {
    // Check output buffer full
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    
    // Debug: log poll attempts occasionally
    static int poll_count = 0;
    static int had_data_count = 0;
    if ((++poll_count % 1000) == 0) {
        kos::lib::serial_write("[KBD-Poll] checked=");
        char buf[12]; int i = 0; uint32_t pc = poll_count;
        while (pc && i < 11) { buf[i++] = '0' + (pc % 10); pc /= 10; }
        while (i--) kos::lib::serial_putc(buf[i]);
        kos::lib::serial_write(" had_data=");
        i = 0; pc = had_data_count;
        if (pc == 0) { kos::lib::serial_write("0"); }
        else { while (pc && i < 11) { buf[i++] = '0' + (pc % 10); pc /= 10; } while (i--) kos::lib::serial_putc(buf[i]); }
        kos::lib::serial_write("\n");
    }
    
    if ((status & 0x01) == 0) return false;
    
    ++had_data_count;
    static bool s_tty_notified = false;
    uint8_t key = ps2.ReadData();
    ::kos::g_kbd_input_source = 2; // mark as polled activity
    ++::kos::g_kbd_events;
    if(handler == 0) return false;
    if (key == 0xE0) { e0Prefix = true; return true; }
    if (key & 0x80) {
        uint8_t code = key & 0x7F;
        if (e0Prefix) { if (code == 0x1D) ctrlRight = false; e0Prefix=false; }
        else if (code == 0x1D) { ctrlLeft = false; }
        return true; // release handled
    }
    if (e0Prefix) {
        e0Prefix = false;
        switch (key) {
            case 0x1D: ctrlRight = true; break;
            case 0x1C: handler->OnKeyDown('\n'); break;
            case 0x49: handler->OnKeyDown((char)0xF1); break;
            case 0x51: handler->OnKeyDown((char)0xF2); break;
            case 0x35: handler->OnKeyDown('/'); break;
            case 0x4A: handler->OnKeyDown('-'); break;
            default: break;
        }
        static bool s_first_poll_logged = false;
        if (!s_first_poll_logged) { Logger::LogKV("KBD", "first-poll"); s_first_poll_logged = true; }
        if (!s_tty_notified) { kos::console::TTY::Write((const int8_t*)"[KBD] using POLL\n"); s_tty_notified = true; }
        ::kos::g_kbd_input_source = 2;
        return true;
    }
    int8_t ch = 0;
    switch(key) {
        case 0x0E: ch='\b'; break; case 0x02: ch='1'; break; case 0x03: ch='2'; break; case 0x04: ch='3'; break; case 0x05: ch='4'; break; case 0x06: ch='5'; break; case 0x07: ch='6'; break; case 0x08: ch='7'; break; case 0x09: ch='8'; break; case 0x0A: ch='9'; break; case 0x0B: ch='0'; break; case 0x0C: ch='-'; break;
        case 0x10: ch='q'; break; case 0x11: ch='w'; break; case 0x12: ch='e'; break; case 0x13: ch='r'; break; case 0x14: ch='t'; break; case 0x15: ch='z'; break; case 0x16: ch='u'; break; case 0x17: ch='i'; break; case 0x18: ch='o'; break; case 0x19: ch='p'; break;
        case 0x1E: ch='a'; break; case 0x1F: ch='s'; break; case 0x20: ch='d'; break; case 0x21: ch='f'; break; case 0x22: ch='g'; break; case 0x23: ch='h'; break; case 0x24: ch='j'; break; case 0x25: ch='k'; break; case 0x26: ch='l'; break;
        case 0x2C: ch='y'; break; case 0x2D: ch='x'; break; case 0x2E: ch='c'; break; case 0x2F: ch='v'; break; case 0x30: ch='b'; break; case 0x31: ch='n'; break; case 0x32: ch='m'; break; case 0x33: ch=','; break; case 0x34: ch='.'; break; case 0x35: ch='-'; break; case 0x4A: ch='-'; break;
        case 0x1C: ch='\n'; break; case 0x39: ch=' '; break; case 0x1D: ctrlLeft=true; break;
        default: break;
    }
    if (ch) {
        if ((ctrlLeft||ctrlRight) && ch >= 'a' && ch <= 'z') ch = (int8_t)(ch - 'a' + 1);
        handler->OnKeyDown(ch);
        static bool s_first_poll_logged = false;
        if (!s_first_poll_logged) { Logger::LogKV("KBD", "first-poll"); s_first_poll_logged = true; }
        if (!s_tty_notified) { kos::console::TTY::Write((const int8_t*)"[KBD] using POLL\n"); s_tty_notified = true; }
        ::kos::g_kbd_input_source = 2;
        ++::kos::g_kbd_events;
    }
    return true;
}