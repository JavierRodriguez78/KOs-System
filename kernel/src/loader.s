.set MAGIC, 0x1badb002
.set FLAGS, (1<<0 | 1<<1)
.set CHECKSUM,  -(MAGIC + FLAGS)

.section .multiboot
    .long MAGIC
    .long FLAGS
    .long CHECKSUM


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
