
// VER https://wiki.osdev.org/Interrupts
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <arch/x86/hardware/interrupts/interrupt_handler.hpp>
#include <arch/x86/hardware/interrupts/interrupt_constants.hpp>
using namespace kos::common;
using namespace kos::arch::x86::hardware::interrupts;

void printf(int8_t* str);
void printfHex(uint8_t);

// Define static TTY declared in header
TTY InterruptManager::tty;

extern "C" {
    // C wrapper called from assembly with stable name
    uint32_t kos_int_handle(uint8_t interrupt, uint32_t esp) {
        return InterruptManager::HandleInterrupt(interrupt, esp);
    }

    // Optional: provide C-visible aliases for certain labels if referenced elsewhere
    void interrupt_ignore();
    // Exception stubs
    void isr_ex_0x00(); void isr_ex_0x01(); void isr_ex_0x02(); void isr_ex_0x03();
    void isr_ex_0x04(); void isr_ex_0x05(); void isr_ex_0x06(); void isr_ex_0x07();
    void isr_ex_0x08(); void isr_ex_0x09(); void isr_ex_0x0A(); void isr_ex_0x0B();
    void isr_ex_0x0C(); void isr_ex_0x0D(); void isr_ex_0x0E(); void isr_ex_0x0F();
    void isr_ex_0x10(); void isr_ex_0x11(); void isr_ex_0x12(); void isr_ex_0x13();
    // IRQ stubs
    void irq_0x00(); void irq_0x01(); void irq_0x02(); void irq_0x03();
    void irq_0x04(); void irq_0x05(); void irq_0x06(); void irq_0x07();
    void irq_0x08(); void irq_0x09(); void irq_0x0A(); void irq_0x0B();
    void irq_0x0C(); void irq_0x0D(); void irq_0x0E(); void irq_0x0F();
    void irq_0x31();
}



// https://wiki.osdev.org/Interrupt_Descriptor_Table
InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[IDT_MAX_INTERRUPTS];
InterruptManager* InterruptManager::ActiveInterruptManager = 0;


void InterruptManager::SetInterruptDescriptorTableEntry(uint8_t interrupt,
    uint16_t CodeSegment, void (*handler)(), uint8_t DescriptorPrivilegeLevel, uint8_t DescriptorType)
{
    // address of pointer to code segment (relative to global descriptor table)
    // and address of the handler (relative to segment)
    interruptDescriptorTable[interrupt].handlerAddressLowBits = ((uint32_t) handler) & 0xFFFF;
    interruptDescriptorTable[interrupt].handlerAddressHighBits = (((uint32_t) handler) >> 16) & 0xFFFF;
    interruptDescriptorTable[interrupt].gdt_codeSegmentSelector = CodeSegment;

    const uint8_t IDT_DESC_PRESENT = 0x80;
    interruptDescriptorTable[interrupt].access = IDT_DESC_PRESENT | ((DescriptorPrivilegeLevel & 3) << 5) | DescriptorType;
    interruptDescriptorTable[interrupt].reserved = 0;
}


InterruptManager::InterruptManager(uint16_t hardwareInterruptOffset, GlobalDescriptorTable* globalDescriptorTable)
        : programmableInterruptControllerMasterCommandPort(PIC1_CMD),
            programmableInterruptControllerMasterDataPort(PIC1_DATA),
            programmableInterruptControllerSlaveCommandPort(PIC2_CMD),
            programmableInterruptControllerSlaveDataPort(PIC2_DATA)
{
    this->hardwareInterruptOffset = hardwareInterruptOffset;
    uint32_t CodeSegment = globalDescriptorTable->CodeSegmentSelector();

    const uint8_t IDT_INTERRUPT_GATE = IDT_TYPE_INTERRUPT_GATE;
    for(uint8_t i = IDT_MAX_INTERRUPTS - 1; i > 0; --i)
    {
        SetInterruptDescriptorTableEntry(i, CodeSegment, &interrupt_ignore, 0, IDT_INTERRUPT_GATE);
        handlers[i] = 0;
    }
    SetInterruptDescriptorTableEntry(EXCEPTION_DIVIDE_ERROR, CodeSegment, &interrupt_ignore, 0, IDT_INTERRUPT_GATE);
    handlers[0] = 0;

    SetInterruptDescriptorTableEntry(EXCEPTION_DIVIDE_ERROR, CodeSegment, &isr_ex_0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DEBUG, CodeSegment, &isr_ex_0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_NMI, CodeSegment, &isr_ex_0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_BREAKPOINT, CodeSegment, &isr_ex_0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_OVERFLOW, CodeSegment, &isr_ex_0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_BOUND_RANGE, CodeSegment, &isr_ex_0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_INVALID_OPCODE, CodeSegment, &isr_ex_0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DEVICE_NOT_AVAILABLE, CodeSegment, &isr_ex_0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_DOUBLE_FAULT, CodeSegment, &isr_ex_0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN, CodeSegment, &isr_ex_0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_INVALID_TSS, CodeSegment, &isr_ex_0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_SEGMENT_NOT_PRESENT, CodeSegment, &isr_ex_0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_STACK_SEGMENT_FAULT, CodeSegment, &isr_ex_0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_GENERAL_PROTECTION_FAULT, CodeSegment, &isr_ex_0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_PAGE_FAULT, CodeSegment, &isr_ex_0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_RESERVED, CodeSegment, &isr_ex_0x0F, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_FPU_ERROR, CodeSegment, &isr_ex_0x10, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_ALIGNMENT_CHECK, CodeSegment, &isr_ex_0x11, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_MACHINE_CHECK, CodeSegment, &isr_ex_0x12, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(EXCEPTION_SIMD_ERROR, CodeSegment, &isr_ex_0x13, 0, IDT_INTERRUPT_GATE);

    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_TIMER, CodeSegment, &irq_0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_KEYBOARD, CodeSegment, &irq_0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_CASCADE, CodeSegment, &irq_0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_SERIAL2, CodeSegment, &irq_0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_SERIAL1, CodeSegment, &irq_0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_PARALLEL2, CodeSegment, &irq_0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_DISKETTE, CodeSegment, &irq_0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_PARALLEL1, CodeSegment, &irq_0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_CMOS, CodeSegment, &irq_0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE1, CodeSegment, &irq_0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE2, CodeSegment, &irq_0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FREE3, CodeSegment, &irq_0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_MOUSE, CodeSegment, &irq_0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_FPU, CodeSegment, &irq_0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_PRIMARY_ATA, CodeSegment, &irq_0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + IRQ_SECONDARY_ATA, CodeSegment, &irq_0x0F, 0, IDT_INTERRUPT_GATE);

    programmableInterruptControllerMasterCommandPort.Write(PIC_ICW1_INIT_WITH_ICW4);
    programmableInterruptControllerSlaveCommandPort.Write(PIC_ICW1_INIT_WITH_ICW4);

    // remap
    programmableInterruptControllerMasterDataPort.Write(hardwareInterruptOffset);
    programmableInterruptControllerSlaveDataPort.Write(hardwareInterruptOffset + 8);

    programmableInterruptControllerMasterDataPort.Write(PIC_ICW3_MASTER_HAS_SLAVE_ON_IR2);
    programmableInterruptControllerSlaveDataPort.Write(PIC_ICW3_SLAVE_IDENTITY_IR2);

    programmableInterruptControllerMasterDataPort.Write(PIC_ICW4_8086_MODE);
    programmableInterruptControllerSlaveDataPort.Write(PIC_ICW4_8086_MODE);

    programmableInterruptControllerMasterDataPort.Write(0x00);
    programmableInterruptControllerSlaveDataPort.Write(0x00);

    InterruptDescriptorTablePointer idt_pointer;
    idt_pointer.size  = IDT_MAX_INTERRUPTS * sizeof(GateDescriptor) - 1;
    idt_pointer.base  = (uint32_t)interruptDescriptorTable;
    asm volatile("lidt %0" : : "m" (idt_pointer));
}

InterruptManager::~InterruptManager()
{
    Deactivate();
}

uint16_t InterruptManager::HardwareInterruptOffset()
{
    return hardwareInterruptOffset;
}

void InterruptManager::Activate()
{
    static TTY tty;
    tty.Write("\n\n=== INTERRUPT MANAGER ACTIVATION ===\n");
    tty.Write("[INT] Activating interrupt manager...\n");
    
    if(ActiveInterruptManager != 0) {
        tty.Write("[INT] Deactivating previous manager\n");
        ActiveInterruptManager->Deactivate();
    }
    
    ActiveInterruptManager = this;
    tty.Write("[INT] Set as active manager\n");
    
    // Check if interrupts are currently enabled
    uint32_t flags;
    asm volatile("pushfl; popl %0" : "=r"(flags));
    tty.Write("[INT] Flags before STI: ");
    tty.WriteHex(flags);
    tty.Write("\n");
    
    asm("sti");
}

void InterruptManager::Deactivate()
{
    if(ActiveInterruptManager == this)
    {
        ActiveInterruptManager = 0;
        asm("cli"); //https://www.felixcloutier.com/x86/cli
    }
}

void InterruptManager::EnableIRQ(uint8_t irq)
{
    if(irq < 8)
    {
        // IRQ 0-7 on master PIC
        uint8_t mask = programmableInterruptControllerMasterDataPort.Read();
        mask &= ~(1 << irq);
        programmableInterruptControllerMasterDataPort.Write(mask);
    }
    else if(irq < 16)
    {
        // IRQ 8-15 on slave PIC
        uint8_t mask = programmableInterruptControllerSlaveDataPort.Read();
        mask &= ~(1 << (irq - 8));
        programmableInterruptControllerSlaveDataPort.Write(mask);
    }
}

void InterruptManager::DisableIRQ(uint8_t irq)
{
    if(irq < 8)
    {
        // IRQ 0-7 on master PIC
        uint8_t mask = programmableInterruptControllerMasterDataPort.Read();
        mask |= (1 << irq);
        programmableInterruptControllerMasterDataPort.Write(mask);
    }
    else if(irq < 16)
    {
        // IRQ 8-15 on slave PIC
        uint8_t mask = programmableInterruptControllerSlaveDataPort.Read();
        mask |= (1 << (irq - 8));
        programmableInterruptControllerSlaveDataPort.Write(mask);
    }
}

uint32_t InterruptManager::HandleInterrupt(uint8_t interrupt, uint32_t esp)
{

    if(ActiveInterruptManager!=0)
        return ActiveInterruptManager->DoHandleInterrupt(interrupt, esp);
    return esp;
}

static inline void print_hex32(uint32_t v) {
    TTY::Write((int8_t*)"0x");
    for (int i = 7; i >= 0; --i) {
        uint8_t nyb = (v >> (i*4)) & 0xF;
        TTY::PutChar(nyb < 10 ? ('0'+nyb) : ('A'+nyb-10));
    }
}

uint32_t InterruptManager::DoHandleInterrupt(uint8_t interrupt, uint32_t esp)
{

    if(handlers[interrupt] !=0)
    {
        esp= handlers[interrupt]->HandleInterrupt(esp);
    }
    else if(interrupt != hardwareInterruptOffset)
    {
        // Exception handling diagnostics for #UD (0x06)
        if (interrupt == 0x06) {
            // Stack layout at entry of our stub (top -> bottom):
            // [gs][fs][es][ds] (16 bytes)
            // [edi][esi][ebp][esp_dump][ebx][edx][ecx][eax] (pusha: 32 bytes)
            // [EIP][CS][EFLAGS] (CPU-pushed for exceptions without error code)
            // For exceptions with error code, there's an extra [ERRCODE] before EIP.
            // For #UD (0x06), no error code.
            uint32_t* s = (uint32_t*)esp;
            const bool hasErrCode = false; // for 0x06
            const uint32_t baseWords = 4 /*segs*/ + 8 /*pusha*/;
            const uint32_t errAdj = hasErrCode ? 1u : 0u;
            uint32_t eip = s[baseWords + errAdj];      // 16+32=48 bytes -> index 12
            uint32_t cs  = s[baseWords + errAdj + 1];
            uint32_t fl  = s[baseWords + errAdj + 2];

            TTY::Write((int8_t*)"#UD Invalid Opcode at ");
            print_hex32(eip);
            TTY::Write((int8_t*)" CS=");
            print_hex32(cs);
            TTY::Write((int8_t*)" EFLAGS=");
            print_hex32(fl);
            TTY::Write((int8_t*)" bytes:");
            uint8_t* p = (uint8_t*)eip;
            for (int i = 0; i < 8; ++i) {
                TTY::PutChar(' ');
                TTY::PutChar("0123456789ABCDEF"[(p[i]>>4)&0xF]);
                TTY::PutChar("0123456789ABCDEF"[p[i]&0xF]);
            }
            TTY::PutChar('\n');
        } else {
            tty.Write("UNHANDLER INTERRUPT 0x");
            tty.WriteHex(interrupt);
        }
    }

    // hardarware interrupts must be acknowleged
    if(hardwareInterruptOffset <= interrupt && interrupt < hardwareInterruptOffset + IRQ_COUNT)
    {
        // Acknowledge slave first if it originated there, then master
        if(hardwareInterruptOffset + 8 <= interrupt)
            programmableInterruptControllerSlaveCommandPort.Write(PIC_EOI);
        programmableInterruptControllerMasterCommandPort.Write(PIC_EOI);
    }
    return esp;
}