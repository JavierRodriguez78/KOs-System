ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

SECTIONS
{
    . = 0x0100000;
    /* Provide kernel start symbol at load address */
    kernel_start = .;
    
    .text : 
    {
        *(.multiboot)
        *(.text*)
        /* text only */
    }
    text_end = .;

    /* Read-only data in its own section for page protection */
    .rodata :
    {
        rodata_start = .;
        *(.rodata*)
    }
    rodata_end = .;
   
    .data :
    {
        data_start = .;
        start_ctors = .;
        KEEP(*( .init_array ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.* )));
        end_ctors = .;
        
        *(.data)
    }
    data_end = .;

    .bss :
    {
        bss_start = .;
        *(.bss)
    }
    bss_end = .;
    /* Kernel end after BSS */
    kernel_end = .;
    /DISCARD/ :
    {
        *(.fini_array*)
        *(.comment)
    }
}