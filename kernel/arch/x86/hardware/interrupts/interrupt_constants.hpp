#pragma once

#include <common/types.hpp>

using namespace kos::common;

namespace kos
{
    namespace arch
    {
        namespace x86
        {
            namespace hardware
            {
                namespace interrupts
                {
                    // Maximum number of interruptions in the IDT (x86)
                    constexpr uint16_t IDT_MAX_INTERRUPTS = 256;

                    // Default offset where hardware interrupts start (remapped PIC)
                    constexpr uint8_t DEFAULT_HARDWARE_INTERRUPT_OFFSET = 0x20;

                    // Number of IRQs (hardware controllers)
                    constexpr uint8_t IRQ_COUNT = 16;
                    constexpr uint8_t IRQ_TIMER = 0x00;
                    constexpr uint8_t IRQ_KEYBOARD = 0x01;
                    constexpr uint8_t IRQ_CASCADE = 0x02;
                    constexpr uint8_t IRQ_SERIAL2 = 0x03;
                    constexpr uint8_t IRQ_SERIAL1 = 0x04;
                    constexpr uint8_t IRQ_PARALLEL2 = 0x05;
                    constexpr uint8_t IRQ_DISKETTE = 0x06;
                    constexpr uint8_t IRQ_PARALLEL1 = 0x07;
                    constexpr uint8_t IRQ_CMOS = 0x08;
                    constexpr uint8_t IRQ_FREE1 = 0x09;
                    constexpr uint8_t IRQ_FREE2 = 0x0A;
                    constexpr uint8_t IRQ_FREE3 = 0x0B;
                    constexpr uint8_t IRQ_MOUSE = 0x0C;
                    constexpr uint8_t IRQ_FPU = 0x0D;
                    constexpr uint8_t IRQ_PRIMARY_ATA = 0x0E;
                    constexpr uint8_t IRQ_SECONDARY_ATA = 0x0F;
                    constexpr uint8_t IRQ_SYSTEM_CALL = 0x31;

                    // PIC I/O port addresses (legacy 8259)
                    constexpr uint16_t PIC1_CMD  = 0x20; // Master PIC command
                    constexpr uint16_t PIC1_DATA = 0x21; // Master PIC data
                    constexpr uint16_t PIC2_CMD  = 0xA0; // Slave PIC command
                    constexpr uint16_t PIC2_DATA = 0xA1; // Slave PIC data

                    // PIC initialization control words (ICW)
                    constexpr uint8_t PIC_ICW1_INIT_WITH_ICW4 = 0x11; // INIT | ICW4-needed
                    constexpr uint8_t PIC_ICW3_MASTER_HAS_SLAVE_ON_IR2 = 0x04; // bitmask for IR line
                    constexpr uint8_t PIC_ICW3_SLAVE_IDENTITY_IR2 = 0x02;      // cascade identity
                    constexpr uint8_t PIC_ICW4_8086_MODE = 0x01;               // 8086/88 (MCS-80/85) mode

                    // PIC End Of Interrupt command
                    constexpr uint8_t PIC_EOI = 0x20;

                    // IDT descriptor flags
                    constexpr uint8_t IDT_DESC_PRESENT = 0x80;
                    constexpr uint8_t IDT_TYPE_INTERRUPT_GATE = 0x0E;

                    // Excepciones de CPU (vector de interrupciones)
                    constexpr uint8_t EXCEPTION_DIVIDE_ERROR = 0x00;
                    constexpr uint8_t EXCEPTION_DEBUG = 0x01;
                    constexpr uint8_t EXCEPTION_NMI = 0x02;
                    constexpr uint8_t EXCEPTION_BREAKPOINT = 0x03;
                    constexpr uint8_t EXCEPTION_OVERFLOW = 0x04;
                    constexpr uint8_t EXCEPTION_BOUND_RANGE = 0x05;
                    constexpr uint8_t EXCEPTION_INVALID_OPCODE = 0x06;
                    constexpr uint8_t EXCEPTION_DEVICE_NOT_AVAILABLE = 0x07;
                    constexpr uint8_t EXCEPTION_DOUBLE_FAULT = 0x08;
                    constexpr uint8_t EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN = 0x09;
                    constexpr uint8_t EXCEPTION_INVALID_TSS = 0x0A;
                    constexpr uint8_t EXCEPTION_SEGMENT_NOT_PRESENT = 0x0B;
                    constexpr uint8_t EXCEPTION_STACK_SEGMENT_FAULT = 0x0C;
                    constexpr uint8_t EXCEPTION_GENERAL_PROTECTION_FAULT = 0x0D;
                    constexpr uint8_t EXCEPTION_PAGE_FAULT = 0x0E;
                    constexpr uint8_t EXCEPTION_RESERVED = 0x0F;
                    constexpr uint8_t EXCEPTION_FPU_ERROR = 0x10;
                    constexpr uint8_t EXCEPTION_ALIGNMENT_CHECK = 0x11;
                    constexpr uint8_t EXCEPTION_MACHINE_CHECK = 0x12;
                    constexpr uint8_t EXCEPTION_SIMD_ERROR = 0x13;
                }
            }
        }
    }
}