.set MAGIC, 0x1badb002
.set FLAGS, (1<<0 | 1<<1)
.set CHECKSUM,  -(MAGIC + FLAGS)

.section .multiboot
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

    # Multiboot2 header (in addition to v1) so GRUB 'multiboot2' works and framebuffer info is passed
    .balign 8
mb2_header_start:
    .long 0xE85250D6        # multiboot2 magic
    .long 0                 # architecture = i386
    .long mb2_header_end - mb2_header_start  # header length
    .long -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) # checksum (sum to zero)

    # Optional framebuffer request tag (type=5). GRUB may choose a close mode.
    .short 5                # type: framebuffer
    .short 0                # flags: 0 (optional)
    .long 20                # size of this tag
    .long 1024              # desired width
    .long 768               # desired height
    .long 32                # desired depth (bpp)
    .balign 8

    # End tag (type=0, size=8)
    .short 0
    .short 0
    .long 8
mb2_header_end:


.section .text
.extern kernelMain
.extern callConstructors
.global loader

loader:
    mov $kernel_stack, %esp
    # Preserve multiboot registers across constructor calls
    mov %eax, %esi   # save multiboot_magic
    mov %ebx, %edi   # save multiboot_info pointer
    call callConstructors
    # Restore and pass args to kernelMain(const void* mbi, uint32_t magic)
    mov %esi, %eax
    mov %edi, %ebx
    push %eax        # push magic (second arg)
    push %ebx        # push mbi (first arg)
    call kernelMain

_stop:
    cli
    hlt
    jmp _stop

.section .bss
.space 2*1024*1024; # 2MB
kernel_stack:
    
    # Mark stack as non-executable for the assembler/linker
    .section .note.GNU-stack,"",@progbits
