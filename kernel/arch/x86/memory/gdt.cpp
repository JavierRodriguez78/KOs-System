//View GlobalDescriptorTable https://wiki.osdev.org/Global_Descriptor_Table

#include <arch/x86/memory/gdt.hpp>
#include <arch/x86/memory/gdt_constants.hpp>


using namespace kos::common;
using namespace kos::arch::x86::memory;


GlobalDescriptorTable::GlobalDescriptorTable():nullSegmentSelector(0, 0, 0),
        unusedSegmentSelector(0, 0, 0),
        codeSegmentSelector(0, GDT_SEGMENT_LIMIT, GDT_CODE_SEGMENT_TYPE),
        dataSegmentSelector(0, GDT_SEGMENT_LIMIT, GDT_DATA_SEGMENT_TYPE)
{
    uint32_t i[2];
    i[1] = (uint32_t)this;
    i[0] = sizeof(GlobalDescriptorTable) << 16;
    asm volatile("lgdt (%0)": :"p" (((uint8_t *) i)+2));
}

GlobalDescriptorTable::~GlobalDescriptorTable()
{
}

uint16_t GlobalDescriptorTable::DataSegmentSelector()
{
    return (uint8_t*)&dataSegmentSelector - (uint8_t*)this;
}

uint16_t GlobalDescriptorTable::CodeSegmentSelector()
{
    return (uint8_t*)&codeSegmentSelector - (uint8_t*)this;
}

GlobalDescriptorTable::SegmentDescriptor::SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t type)
{
    uint8_t* target = (uint8_t*)this;

    if (limit <= GDT_LIMIT_16BIT_MAX)
    {
        // 16-bit address space
        target[6] = GDT_FLAG_16BIT;
    }
    else
    {
        // 32-bit address space
        // Now we have to squeeze the (32-bit) limit into 2.5 regiters (20-bit).
        // This is done by discarding the 12 least significant bits, but this
        // is only legal, if they are all ==1, so they are implicitly still there

        // so if the last bits aren't all 1, we have to set them to 1, but this
        // would increase the limit (cannot do that, because we might go beyond
        // the physical limit or get overlap with other segments) so we have to
        // compensate this by decreasing a higher bit (and might have up to
        // 4095 wasted bytes behind the used memory)

        if((limit & GDT_PAGE_MASK) != GDT_PAGE_MASK)
            limit = (limit >> GDT_PAGE_SHIFT)-1;
        else
            limit = limit >> GDT_PAGE_SHIFT;

        target[6] = GDT_FLAG_32BIT;
    }

    // Encode the limit
    target[0] = limit & 0xFF;
    target[1] = (limit >> 8) & 0xFF;
    target[6] |= (limit >> 16) & 0xF;

    // Encode the base
    target[2] = base & 0xFF;
    target[3] = (base >> 8) & 0xFF;
    target[4] = (base >> 16) & 0xFF;
    target[7] = (base >> 24) & 0xFF;

    // Type
    target[5] = type;
}

uint32_t GlobalDescriptorTable::SegmentDescriptor::Base()
{
    uint8_t* target = (uint8_t*)this;

    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];

    return result;
}

uint32_t GlobalDescriptorTable::SegmentDescriptor::Limit()
{
    uint8_t* target = (uint8_t*)this;

    uint32_t result = target[6] & 0xF;
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];

    if((target[6] & GDT_FLAG_32BIT) == GDT_FLAG_32BIT)
        result = (result << GDT_PAGE_SHIFT) | GDT_PAGE_MASK;

    return result;
}
