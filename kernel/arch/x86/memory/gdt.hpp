#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__MEMORY__GDT_H
#define __KOS__ARCH__X86__HARDWARE__MEMORY__GDT_H

#include <common/types.hpp>
#include <arch/x86/memory/gdt_constants.hpp>

using namespace kos::common;

namespace kos{
    namespace arch{
        namespace x86{
            namespace memory{
                
                /*
                *@brief Represents the Global Descriptor Table (GDT) for the x86 architecture.
                */
                class GlobalDescriptorTable
                {
                    
                    public:

                        /**
                        * @brief Represents a single segment descriptor within the GDT.
                        */
                        class SegmentDescriptor
                        {
                            private:
                                uint16_t limit_lo; // Limit bits 0-15
                                uint16_t base_lo; // Base bits 0-15
                                uint8_t base_hi; // Base bits 16-23
                                uint8_t type; // Type and attributes
                                uint8_t limit_hi; // Limit bits 16-19
                                uint8_t base_vhi; // Base bits 24-31

                            public:
                                /*
                                * @brief Constructs a new SegmentDescriptor.
                                * @param base The base address of the segment.
                                * @param limit The limit of the segment.
                                * @param type The type and attributes of the segment.
                                */
                                SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t type);

                                /*
                                * @brief Returns the base address of the segment.
                                * @return The base address.
                                */
                                uint32_t Base();

                                /*
                                * @brief Returns the limit of the segment.
                                * @return The limit.
                                */
                                uint32_t Limit();
                        } __attribute__((packed));

                    private:
                        SegmentDescriptor nullSegmentSelector; // Null segment
                        SegmentDescriptor unusedSegmentSelector; // Unused segment
                        SegmentDescriptor codeSegmentSelector; // Code segment
                        SegmentDescriptor dataSegmentSelector; // Data segment

                    public:
                        /*
                        * @brief Constructs a new GlobalDescriptorTable.
                        * Uses constants defined in gdt_constants.hpp for types and limits.
                        */
                        GlobalDescriptorTable();
                        
                        /*
                        * @brief Destroys the GlobalDescriptorTable.
                        */
                        ~GlobalDescriptorTable();

                        /*
                        * @brief Returns the code segment selector.
                        * @return The code segment selector.
                        */
                        uint16_t CodeSegmentSelector();

                        /*
                        * @brief Returns the data segment selector.
                        * @return The data segment selector.
                        */
                        uint16_t DataSegmentSelector();
                };
            }
        }
    }
}
#endif