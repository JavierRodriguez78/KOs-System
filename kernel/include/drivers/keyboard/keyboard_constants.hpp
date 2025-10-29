#pragma once

// Keyboard I/O port addresses
#define KBD_DATA_PORT         0x60
#define KBD_COMMAND_PORT      0x64

// Keyboard interrupt vector
#define KBD_INTERRUPT_VECTOR  0x21

// PS/2 Controller Commands
#define KBD_CMD_DISABLE_FIRST_PORT   0xAD
#define KBD_CMD_DISABLE_SECOND_PORT  0xA7
#define KBD_CMD_READ_CONFIG          0x20
#define KBD_CMD_WRITE_CONFIG         0x60
#define KBD_CMD_ENABLE_FIRST_PORT    0xAE
#define KBD_CMD_ENABLE_SCANNING      0xF4

// Keyboard scancode constants
#define KBD_SCANCODE_EXTENDED_PREFIX 0xE0
#define KBD_SCANCODE_BREAK_CODE      0x80
#define KBD_SCANCODE_BACKSPACE       0x0E
#define KBD_SCANCODE_ENTER           0x1C
#define KBD_SCANCODE_SPACE           0x39

// Example: Numeric keys (main row)
#define KBD_SCANCODE_1   0x02
#define KBD_SCANCODE_2   0x03
#define KBD_SCANCODE_3   0x04
#define KBD_SCANCODE_4   0x05
#define KBD_SCANCODE_5   0x06
#define KBD_SCANCODE_6   0x07
#define KBD_SCANCODE_7   0x08
#define KBD_SCANCODE_8   0x09
#define KBD_SCANCODE_9   0x0A
#define KBD_SCANCODE_0   0x0B
#define KBD_SCANCODE_MINUS_MAIN 0x0C
#define KBD_SCANCODE_MINUS_KEYPAD 0x4A
#define KBD_SCANCODE_SLASH_KEYPAD 0x35

// Timeout for controller operations
#define KBD_CONTROLLER_TIMEOUT 1000

// Config bits
#define KBD_CONFIG_ENABLE_FIRST_PORT_INTERRUPT 0x01
#define KBD_CONFIG_DISABLE_FIRST_PORT          0x10
#define KBD_CONFIG_DISABLE_SECOND_PORT         0x20
