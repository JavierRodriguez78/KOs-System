#ifndef __KOS__GDT_H
#define __KOS__GDT_H

#include <common/types.hpp>

namespace kos{
    class GlobalDescriptorTable
    {
        public:

            class SegmentDescriptor
            {
                private:
                    kos::common::uint16_t limit_lo;
                    kos::common::uint16_t base_lo;
                    kos::common::uint8_t base_hi;
                    kos::common::uint8_t type;
                    kos::common::uint8_t limit_hi;
                    kos::common::uint8_t base_vhi;

                public:
                    SegmentDescriptor(kos::common::uint32_t base, kos::common::uint32_t limit, kos::common::uint8_t type);
                    kos::common::uint32_t Base();
                    kos::common::uint32_t Limit();
            } __attribute__((packed));

        private:
            SegmentDescriptor nullSegmentSelector;
            SegmentDescriptor unusedSegmentSelector;
            SegmentDescriptor codeSegmentSelector;
            SegmentDescriptor dataSegmentSelector;

        public:

            GlobalDescriptorTable();
            ~GlobalDescriptorTable();

            kos::common::uint16_t CodeSegmentSelector();
            kos::common::uint16_t DataSegmentSelector();
    };
}
#endif