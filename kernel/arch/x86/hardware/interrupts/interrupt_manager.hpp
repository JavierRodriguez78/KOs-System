//https://www.lowlevel.eu/wiki/Tyndur

#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__INTERRUPTS__INTERRUPTMANAGER_H
#define __KOS__ARCH__X86__HARDWARE__INTERRUPTS__INTERRUPTMANAGER_H

#include <memory/gdt.hpp>
#include <common/types.hpp>
#include <hardware/port.hpp>
#include <console/tty.hpp>
#include <arch/x86/hardware/interrupts/interrupt_constants.hpp>


// Forward declare to break header cycle; full definition in interrupt_handler.hpp
namespace kos { 
    namespace arch { 
        namespace x86 { 
            namespace hardware { 
                namespace interrupts { 
                    class InterruptHandler; 
                }
            }
        }
    }
}

using namespace kos::common;
using namespace kos::console;
using namespace kos::hardware;


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

                    /**
                     * @brief Main class for managing CPU interrupts and the IDT
                    */
                    class InterruptManager
                    {
                        friend class InterruptHandler;
                        protected:
                            /* Currently active manager */
                            static InterruptManager* ActiveInterruptManager;
                            
                            /* Table of interrupt handlers */
                            InterruptHandler* handlers[IDT_MAX_INTERRUPTS];

                            /**
                            * @brief IDT entry descriptor
                            */
                            struct GateDescriptor
                            {
                                uint16_t handlerAddressLowBits;
                                uint16_t gdt_codeSegmentSelector;
                                uint8_t reserved;
                                uint8_t access;
                                uint16_t handlerAddressHighBits;
                            } __attribute__((packed));

                            /* Complete IDT */
                            static GateDescriptor interruptDescriptorTable[IDT_MAX_INTERRUPTS];

                            /*
                            * @brief Interrupt Descriptor Table Pointer
                            */
                            struct InterruptDescriptorTablePointer
                            {
                                uint16_t size;
                                uint32_t base;
                            } __attribute__((packed));

                            /* Hardware IRQ offset */
                            uint16_t hardwareInterruptOffset;

                            /**
                            * @brief Sets a single IDT entry
                            * @param interrupt Interrupt number
                            * @param CodeSegment GDT code segment selector
                            * @param handler Handler function pointer
                            * @param DescriptorPrivilegeLevel Descriptor privilege (0-3)
                            * @param DescriptorType Descriptor type (interrupt/trap gate)
                            */     
                            static void SetInterruptDescriptorTableEntry(uint8_t interrupt,
                                uint16_t codeSegmentSelectorOffset, void (*handler)(),
                                uint8_t DescriptorPrivilegeLevel, uint8_t DescriptorType);


                            /* Empty interrupt that does nothing */
                            static void InterruptIgnore();

                            /** Specific hardware IRQ handlers */
                            static void HandleInterruptRequest0x00();
                            static void HandleInterruptRequest0x01();
                            static void HandleInterruptRequest0x02();
                            static void HandleInterruptRequest0x03();
                            static void HandleInterruptRequest0x04();
                            static void HandleInterruptRequest0x05();
                            static void HandleInterruptRequest0x06();
                            static void HandleInterruptRequest0x07();
                            static void HandleInterruptRequest0x08();
                            static void HandleInterruptRequest0x09();
                            static void HandleInterruptRequest0x0A();
                            static void HandleInterruptRequest0x0B();
                            static void HandleInterruptRequest0x0C();
                            static void HandleInterruptRequest0x0D();
                            static void HandleInterruptRequest0x0E();
                            static void HandleInterruptRequest0x0F();
                            static void HandleInterruptRequest0x31();

                            /** CPU exception handlers */
                            static void HandleException0x00();
                            static void HandleException0x01();
                            static void HandleException0x02();
                            static void HandleException0x03();
                            static void HandleException0x04();
                            static void HandleException0x05();
                            static void HandleException0x06();
                            static void HandleException0x07();
                            static void HandleException0x08();
                            static void HandleException0x09();
                            static void HandleException0x0A();
                            static void HandleException0x0B();
                            static void HandleException0x0C();
                            static void HandleException0x0D();
                            static void HandleException0x0E();
                            static void HandleException0x0F();
                            static void HandleException0x10();
                            static void HandleException0x11();
                            static void HandleException0x12();
                            static void HandleException0x13();

                            
                            /* Internal delegated interrupt handling */
                            uint32_t DoHandleInterrupt(uint8_t interrupt, uint32_t esp);

                            /* PIC ports */
                            Port8BitSlow programmableInterruptControllerMasterCommandPort;
                            Port8BitSlow programmableInterruptControllerMasterDataPort;
                            Port8BitSlow programmableInterruptControllerSlaveCommandPort;
                            Port8BitSlow programmableInterruptControllerSlaveDataPort;

                        public:
                            
                            /**
                            * @brief Constructor: Initializes PIC and IDT
                            * @param hardwareInterruptOffset Offset for hardware interrupts
                            * @param globalDescriptorTable Pointer to GDT
                            */
                            InterruptManager(uint16_t hardwareInterruptOffset, kos::GlobalDescriptorTable* globalDescriptorTable);
                            
                            /**
                             * @brief Destructor: Cleans up resources   
                             */
                            ~InterruptManager();

                            /**
                             * @brief Get hardware interrupt offset 
                             */
                            uint16_t HardwareInterruptOffset();
                            
                            /** 
                             * @brief Activate this interrupt manager
                             */
                            void Activate();

                            /**
                             * @brief Deactivate this interrupt manager
                             */
                            void Deactivate();

                            /**
                             * @brief Enable a specific IRQ
                             * @param irq IRQ number to enable
                             */
                            void EnableIRQ(uint8_t irq);

                            /**
                             * @brief Disable a specific IRQ
                             * @param irq IRQ number to disable
                             */
                            void DisableIRQ(uint8_t irq);

                            /**
                             * @brief Register an interrupt handler
                             * @param interrupt Interrupt number
                             * @param handler Pointer to handler
                             * Entry point called from ISR stubs
                            */
                            static uint32_t HandleInterrupt(uint8_t interrupt, uint32_t esp);

                        private:
                            
                            static TTY tty;
                    };
                }
            }
        }
    }
}
#endif