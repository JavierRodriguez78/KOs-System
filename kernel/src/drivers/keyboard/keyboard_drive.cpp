#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/ps2/ps2.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <kernel/globals.hpp>
#include <kernel/input_debug.hpp>
#include <lib/serial.hpp>
#include <input/event_queue.hpp>
using namespace kos::drivers::keyboard;
using namespace kos::console;

namespace {
static inline uint32_t kbd_modifiers(bool ctrlLeft, bool ctrlRight) {
    uint32_t mods = 0;
    if (ctrlLeft || ctrlRight) mods |= 0x01u;
    return mods;
}

static inline void enqueue_key_event(kos::input::EventType type, uint8_t keyCode, uint32_t modifiers) {
    kos::input::InputEvent ev{};
    ev.type = type;
    ev.timestamp_ms = 0;
    ev.target_window = 0;
    ev.key_data.key_code = keyCode;
    ev.key_data.modifiers = modifiers;
    (void)kos::input::InputEventQueue::Instance().Enqueue(ev);
}
}

// Complete PS/2 Keyboard Set 2 scancode map for make (press) codes only.
// This is the primary input mode after keyboard reset; using full table for reliability.
static int8_t map_set2_make(uint8_t key) {
    switch (key) {
        // Function keys - silently ignore for now (return 0)
        case 0x07: return 0;  // F12
        case 0x0B: return 0;  // F10 (not mapped)
        case 0x0C: return 0;  // F9  (not mapped)
        case 0x05: return 0;  // F4  (not mapped)
        case 0x06: return 0;  // F5  (not mapped)
        case 0x04: return 0;  // F3  (not mapped)

        // Control keys
        case 0x0D: return 0x09;  // Tab
        case 0x66: return '\b';  // Backspace
        case 0x5A: return '\n';  // Enter
        case 0x76: return 0x1B;  // Escape
        case 0x29: return ' ';   // Space

        // Main number row (top)
        case 0x16: return '1'; case 0x1E: return '2'; case 0x26: return '3'; case 0x25: return '4';
        case 0x2E: return '5'; case 0x36: return '6'; case 0x3D: return '7'; case 0x3E: return '8';
        case 0x46: return '9'; case 0x45: return '0';
        
        // Symbols on number row
        case 0x4E: return '-';  // Minus/underscore
        case 0x55: return '=';  // Equal/plus
        case 0x5D: return '\\'; // Backslash/pipe

        // QWERTY top row
        case 0x15: return 'q'; case 0x1D: return 'w'; case 0x24: return 'e'; case 0x2D: return 'r';
        case 0x2C: return 't'; case 0x35: return 'y'; case 0x3C: return 'u'; case 0x43: return 'i';
        case 0x44: return 'o'; case 0x4D: return 'p';

        // Bracket symbols
        case 0x54: return '[';  // Left bracket
        case 0x5B: return ']';  // Right bracket

        // ASDF middle row
        case 0x1C: return 'a'; case 0x1B: return 's'; case 0x23: return 'd'; case 0x2B: return 'f';
        case 0x34: return 'g'; case 0x33: return 'h'; case 0x3B: return 'j'; case 0x42: return 'k';
        case 0x4B: return 'l';

        // Punctuation symbols
        case 0x4C: return ';';  // Semicolon/colon
        case 0x52: return '\''; // Apostrophe/quote

        // ZXCV bottom row
        case 0x1A: return 'z'; case 0x22: return 'x'; case 0x21: return 'c'; case 0x2A: return 'v';
        case 0x32: return 'b'; case 0x31: return 'n'; case 0x3A: return 'm';

        // Punctuation/symbols
        case 0x41: return ',';  // Comma/less
        case 0x49: return '.';  // Period/greater
        case 0x4A: return '/';  // Forward slash/question

        // Modifier keys (track state but don't deliver character)
        case 0x12: return 0;  // Left Shift  
        case 0x59: return 0;  // Right Shift
        case 0x14: return 0;  // Left Ctrl
        case 0x11: return 0;  // Left Alt
        
        // PrintScreen, ScrollLock, Pause - ignore
        case 0x7E: return 0;  // ScrollLock (with E0)
        case 0x77: return 0;  // Pause
        
        // Unrecognized codes - return 0 (will be handled by SET 1 path if enabled)
        default: return 0;
    }
}


KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler)
:InterruptHandler(manager, 0x21),dataport(0x60),commandport(0x64){
    this->handler = handler;
    // PS/2 controller already initialized in InitDrivers
};


KeyboardDriver::~KeyboardDriver()
{

};

void KeyboardDriver::SetHandler(KeyboardEventHandler* newHandler) {
    handler = newHandler;
#if KOS_INPUT_DEBUG
    // Debug log
    kos::lib::serial_write("[KBD] SetHandler: old handler replaced, new ptr=");
    const char* hex = "0123456789ABCDEF";
    uintptr_t addr = (uintptr_t)newHandler;
    for (int i = 7; i >= 0; --i) {
        kos::lib::serial_putc(hex[(addr >> (i*4)) & 0xF]);
    }
    kos::lib::serial_write("\n");
#endif
}


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
#if KOS_INPUT_DEBUG
    // Immediate log at entry
    kos::lib::serial_write("!");
#endif
    
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t key = ps2.ReadData();

#if KOS_INPUT_DEBUG
    const char* hex = "0123456789ABCDEF";
#endif
    
    // Filter out ACK and other special bytes that should not be processed as scancodes
    // ACK (0xFA), RESEND (0xFE), and SELF_TEST_OK (0xAA) are responses to commands, not scancodes
    if (key == 0xFA || key == 0xFE || key == 0xAA) {
#if KOS_INPUT_DEBUG
        kos::lib::serial_write("[I]");
        kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
        kos::lib::serial_putc(hex[key & 0xF]);
        if (key == 0xFA) kos::lib::serial_write("=ACK");
        else if (key == 0xFE) kos::lib::serial_write("=RESEND");
        else if (key == 0xAA) kos::lib::serial_write("=SELF_TEST_OK");
        kos::lib::serial_write(" (ignored)\n");
#endif
        return esp;  // Ignore and return immediately
    }
    
    // Mark that we received actual keyboard activity (scancodes/releases, not control bytes)
    ::kos::g_kbd_input_source = 1;
    ++::kos::g_kbd_events;
    
#if KOS_INPUT_DEBUG
    // Log the scancode
    kos::lib::serial_write("[I]");
    kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
    kos::lib::serial_putc(hex[key & 0xF]);
    
    // Extra info: log if it's a special scancode
    if (key == 0xE0) kos::lib::serial_write("=EXTENDED");
    else if (key >= 0x80 && key < 0xE0) kos::lib::serial_write("=RELEASE");
    
    kos::lib::serial_write("\n");
#endif

    // Use global handler override if set, otherwise use instance handler
    KeyboardEventHandler* activeHandler = ::kos::g_keyboard_handler_override ? ::kos::g_keyboard_handler_override : handler;
    
    if(activeHandler == 0)
        return esp;

    // Handle set-2 break prefix: F0 <make-code>
    // SET 2 is still used for break codes but primary char decoding is SET 1
    static bool s_set2_break = false;
    if (key == 0xF0) {
        s_set2_break = true;
        return esp;
    }
    if (s_set2_break) {
        s_set2_break = false;
        return esp;
    }

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
                    activeHandler->OnKeyDown('\n');
                    enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'\n', kbd_modifiers(ctrlLeft, ctrlRight));
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
                    break;
                case 0x49: // Page Up (E0 49)
                    activeHandler->OnKeyDown((char)0xF1); // internal code for PageUp
                    enqueue_key_event(kos::input::EventType::KeyPress, 0xF1u, kbd_modifiers(ctrlLeft, ctrlRight));
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
                    break;
                case 0x51: // Page Down (E0 51)
                    activeHandler->OnKeyDown((char)0xF2); // internal code for PageDown
                    enqueue_key_event(kos::input::EventType::KeyPress, 0xF2u, kbd_modifiers(ctrlLeft, ctrlRight));
                    {
                        static bool s_first_irq_logged = false;
                        if (!s_first_irq_logged) { Logger::LogKV("KBD", "first-irq"); s_first_irq_logged = true; }
                    }
                    break;
                case 0x35:
                    activeHandler->OnKeyDown('/');
                    enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'/', kbd_modifiers(ctrlLeft, ctrlRight));
                    break; // Keypad '/' (E0 35)
                case 0x4A:
                    activeHandler->OnKeyDown('-');
                    enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'-', kbd_modifiers(ctrlLeft, ctrlRight));
                    break; // Keypad '-' (E0 4A) on some layouts
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
            // Tab
            case 0x0F: ch = 0x09; break;
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
            case 0x15: ch = 'y'; break;  // US QWERTY layout
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

            case 0x2C: ch = 'z'; break;  // US QWERTY layout
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
                // Log unmapped scancodes for debugging
#if KOS_INPUT_DEBUG
                if (key < 0x60) {  // Only log likely keyboard scancodes, not special codes
                    kos::lib::serial_write("[KBD] Unmapped scancode: 0x");
                    const char* hex = "0123456789ABCDEF";
                    kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
                    kos::lib::serial_putc(hex[key & 0xF]);
                    kos::lib::serial_write("\n");
                }
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
#if KOS_INPUT_DEBUG
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
#endif
            
            activeHandler->OnKeyDown(ch);
            enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)ch, kbd_modifiers(ctrlLeft, ctrlRight));
            
            // Debug: verify handler isn't null
#if KOS_INPUT_DEBUG
            static int handler_check = 0;
            if (handler_check++ < 3) {
                kos::lib::serial_write("[KBD] activeHandler ptr=");
                const char* hex = "0123456789ABCDEF";
                uintptr_t addr = (uintptr_t)activeHandler;
                for (int i = 7; i >= 0; --i) {
                    kos::lib::serial_putc(hex[(addr >> (i*4)) & 0xF]);
                }
                kos::lib::serial_write("\n");
            }
#endif
            
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
    // Don't poll if keyboard initialization hasn't completed yet
    if (!::kos::g_kbd_poll_enabled) return false;
    
    // Check output buffer full
    auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
    uint8_t status = ps2.ReadStatus();
    
    // Debug: log poll attempts more frequently to diagnose PS/2 status
#if KOS_INPUT_DEBUG
    static int poll_count = 0;
    static int had_data_count = 0;
    static int aux_skip_count = 0;
    static int obf_seen = 0;
#endif
#if KOS_INPUT_DEBUG
    const char* hex = "0123456789ABCDEF";
#endif
    
    // Log every poll status to understand what's happening
#if KOS_INPUT_DEBUG
    if ((++poll_count % 100) == 0) {
        // Check if OBF was ever seen
        if (status & 0x01) {
            ++obf_seen;
        }
        kos::lib::serial_write("[KBD-Poll] check#");
        char buf[12]; int i = 0; uint32_t pc = poll_count;
        while (pc && i < 11) { buf[i++] = '0' + (pc % 10); pc /= 10; }
        while (i--) kos::lib::serial_putc(buf[i]);
        kos::lib::serial_write(" status=0x");
        kos::lib::serial_putc(hex[(status >> 4) & 0xF]);
        kos::lib::serial_putc(hex[status & 0xF]);
        kos::lib::serial_write(" obf_freq=");
        i = 0; pc = obf_seen;
        if (pc == 0) { kos::lib::serial_write("0"); }
        else { while (pc && i < 11) { buf[i++] = '0' + (pc % 10); pc /= 10; } while (i--) kos::lib::serial_putc(buf[i]); }
        kos::lib::serial_write("\n");
    }
#endif
    
    // Check if output buffer has data (bit 0)
    if ((status & 0x01) == 0) return false;
    
    // If AUX is set the pending byte belongs to the mouse.
    // Forward one byte to mouse polling so keyboard polling does not stall
    // behind a mouse byte at the head of the shared PS/2 output buffer.
    if (status & 0x20) {
#if KOS_INPUT_DEBUG
        ++aux_skip_count;
#endif
        if (::kos::g_mouse_driver_ptr) {
            ::kos::g_mouse_driver_ptr->PollOnce();
            return true; // continue draining this tick
        }
        return false;
    }
    
#if KOS_INPUT_DEBUG
    ++had_data_count;
#endif
#if KOS_INPUT_DEBUG
    static bool s_tty_notified = false;
#endif
    uint8_t key = ps2.ReadData();
    
    // Filter out ACK and other special bytes - these should not be processed as scancodes
    if (key == 0xFA || key == 0xFE || key == 0xAA) {
#if KOS_INPUT_DEBUG
        kos::lib::serial_write("[P]");
        kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
        kos::lib::serial_putc(hex[key & 0xF]);
        if (key == 0xFA) kos::lib::serial_write("=ACK");
        else if (key == 0xFE) kos::lib::serial_write("=RESEND");
        else if (key == 0xAA) kos::lib::serial_write("=SELF_TEST_OK");
        kos::lib::serial_write(" (ignored)\n");
#endif
        return true; // Continue polling to drain other data
    }
    
    // ALWAYS log polled keys
#if KOS_INPUT_DEBUG
    kos::lib::serial_write("[P]");
    kos::lib::serial_putc(hex[(key >> 4) & 0xF]);
    kos::lib::serial_putc(hex[key & 0xF]);
    kos::lib::serial_write("\n");
#endif
    ::kos::g_kbd_input_source = 2; // mark as polled activity
    ++::kos::g_kbd_events;
    
    // Use global handler override if set, otherwise use instance handler
    KeyboardEventHandler* activeHandler = ::kos::g_keyboard_handler_override ? ::kos::g_keyboard_handler_override : handler;
    
    if(activeHandler == 0) return false;

    // Handle set-2 break prefix in polling path as well.
    // SET 2 is still used for break codes but primary char decoding is SET 1
    static bool s_set2_break_poll = false;
    if (key == 0xF0) { s_set2_break_poll = true; return true; }
    if (s_set2_break_poll) { s_set2_break_poll = false; return true; }

    // Skip to SET 1 decoding (primary method) - avoid redundant SET 2 false decoding

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
            case 0x1C:
                activeHandler->OnKeyDown('\n');
                enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'\n', kbd_modifiers(ctrlLeft, ctrlRight));
                break;
            case 0x49:
                activeHandler->OnKeyDown((char)0xF1);
                enqueue_key_event(kos::input::EventType::KeyPress, 0xF1u, kbd_modifiers(ctrlLeft, ctrlRight));
                break;
            case 0x51:
                activeHandler->OnKeyDown((char)0xF2);
                enqueue_key_event(kos::input::EventType::KeyPress, 0xF2u, kbd_modifiers(ctrlLeft, ctrlRight));
                break;
            case 0x35:
                activeHandler->OnKeyDown('/');
                enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'/', kbd_modifiers(ctrlLeft, ctrlRight));
                break;
            case 0x4A:
                activeHandler->OnKeyDown('-');
                enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)'-', kbd_modifiers(ctrlLeft, ctrlRight));
                break;
            default: break;
        }
        static bool s_first_poll_logged = false;
        if (!s_first_poll_logged) { Logger::LogKV("KBD", "first-poll"); s_first_poll_logged = true; }
#if KOS_INPUT_DEBUG
        if (!s_tty_notified) { kos::console::TTY::Write((const int8_t*)"[KBD] using POLL\n"); s_tty_notified = true; }
#endif
        ::kos::g_kbd_input_source = 2;
        return true;
    }
    int8_t ch = 0;
    switch(key) {
        case 0x0E: ch='\b'; break; case 0x0F: ch=0x09; break; // Tab
        case 0x02: ch='1'; break; case 0x03: ch='2'; break; case 0x04: ch='3'; break; case 0x05: ch='4'; break; case 0x06: ch='5'; break; case 0x07: ch='6'; break; case 0x08: ch='7'; break; case 0x09: ch='8'; break; case 0x0A: ch='9'; break; case 0x0B: ch='0'; break; case 0x0C: ch='-'; break;
        case 0x10: ch='q'; break; case 0x11: ch='w'; break; case 0x12: ch='e'; break; case 0x13: ch='r'; break; case 0x14: ch='t'; break; case 0x15: ch='y'; break; case 0x16: ch='u'; break; case 0x17: ch='i'; break; case 0x18: ch='o'; break; case 0x19: ch='p'; break;
        case 0x1E: ch='a'; break; case 0x1F: ch='s'; break; case 0x20: ch='d'; break; case 0x21: ch='f'; break; case 0x22: ch='g'; break; case 0x23: ch='h'; break; case 0x24: ch='j'; break; case 0x25: ch='k'; break; case 0x26: ch='l'; break;
        case 0x2C: ch='z'; break; case 0x2D: ch='x'; break; case 0x2E: ch='c'; break; case 0x2F: ch='v'; break; case 0x30: ch='b'; break; case 0x31: ch='n'; break; case 0x32: ch='m'; break; case 0x33: ch=','; break; case 0x34: ch='.'; break; case 0x35: ch='-'; break; case 0x4A: ch='-'; break;
        case 0x1C: ch='\n'; break; case 0x39: ch=' '; break; case 0x1D: ctrlLeft=true; break;
        default: break;
    }
    if (ch) {
        if ((ctrlLeft||ctrlRight) && ch >= 'a' && ch <= 'z') ch = (int8_t)(ch - 'a' + 1);
        activeHandler->OnKeyDown(ch);
        enqueue_key_event(kos::input::EventType::KeyPress, (uint8_t)ch, kbd_modifiers(ctrlLeft, ctrlRight));
        static bool s_first_poll_logged = false;
        if (!s_first_poll_logged) { Logger::LogKV("KBD", "first-poll"); s_first_poll_logged = true; }
    #if KOS_INPUT_DEBUG
        if (!s_tty_notified) { kos::console::TTY::Write((const int8_t*)"[KBD] using POLL\n"); s_tty_notified = true; }
    #endif
        ::kos::g_kbd_input_source = 2;
        ++::kos::g_kbd_events;
    }
    return true;
}