ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

/* Program headers to avoid RWX PT_LOAD warnings */
PHDRS
{
    text   PT_LOAD FLAGS(4 | 1);  /* PF_R | PF_X */
    rodata PT_LOAD FLAGS(4);      /* PF_R */
    data   PT_LOAD FLAGS(4 | 2);  /* PF_R | PF_W */
}

SECTIONS
{
    /* Load kernel at 1 MiB */
    . = 0x0100000;

    /* Provide kernel start symbol at load address */
    kernel_start = .;

    /* Code (RX) */
    .text ALIGN(4K) :
    {
        *(.multiboot)
        *(.text*)
        /* text only */
    } :text
    text_end = .;

    /* Read-only data (R) */
    .rodata ALIGN(4K) :
    {
        rodata_start = .;
        *(.rodata*)
    } :rodata
    rodata_end = .;

    /* Writable data (RW) */
    .data ALIGN(4K) :
    {
        data_start = .;
        start_ctors = .;
        KEEP(*( .init_array ))
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.* )))
        end_ctors = .;

        *(.data)
    } :data
    data_end = .;

    /* BSS (no file contents), still mapped RW */
    .bss ALIGN(4K) (NOLOAD) :
    {
        bss_start = .;
        *(.bss)
        *(COMMON)
    } :data
    bss_end = .;

    /* Kernel end after BSS */
    kernel_end = .;

    /DISCARD/ :
    {
        *(.fini_array*)
        *(.comment)
    }
}