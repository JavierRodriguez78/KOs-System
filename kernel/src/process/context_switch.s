# Context switching assembly routines for the KOS scheduler
# These routines handle saving and restoring CPU context for task switching

.section .text

# void Scheduler::SwitchContext(CPUContext* old_context, CPUContext* new_context)
# This function performs the actual context switch between tasks
.global _ZN3kos6kernel9Scheduler13SwitchContextEPNS1_10CPUContextES3_

_ZN3kos6kernel9Scheduler13SwitchContextEPNS1_10CPUContextES3_:
    # Function parameters:
    # 4(%esp) = old_context (CPUContext*)
    # 8(%esp) = new_context (CPUContext*)
    
    # Save current CPU state to old_context
    movl 4(%esp), %eax      # Load old_context pointer into EAX
    
    # Save general purpose registers (in CPUContext order)
    movl %edi, 0(%eax)      # Save EDI
    movl %esi, 4(%eax)      # Save ESI  
    movl %ebp, 8(%eax)      # Save EBP
    movl %esp, 12(%eax)     # Save ESP (note: esp_dump field)
    movl %ebx, 16(%eax)     # Save EBX
    movl %edx, 20(%eax)     # Save EDX
    movl %ecx, 24(%eax)     # Save ECX
    # Don't save EAX here as we're using it
    
    # Save segment registers
    movw %ds, 32(%eax)      # Save DS
    movw %es, 36(%eax)      # Save ES
    movw %fs, 40(%eax)      # Save FS
    movw %gs, 44(%eax)      # Save GS
    
    # Save EIP (return address from this function call)
    movl (%esp), %edx       # Get return address from stack
    movl %edx, 48(%eax)     # Save EIP
    
    # Save CS (code segment)
    movw %cs, 52(%eax)      # Save CS
    
    # Save EFLAGS
    pushf                   # Push flags onto stack
    popl %edx               # Pop flags into EDX
    movl %edx, 56(%eax)     # Save EFLAGS
    
    # Save ESP (current stack pointer)
    movl %esp, 60(%eax)     # Save ESP
    
    # Save SS (stack segment)
    movw %ss, 64(%eax)      # Save SS
    
    # Now save EAX (we can do this now that we're done using it)
    movl %eax, %edx         # Temporarily store old_context in EDX
    movl 28(%edx), %ecx     # Load the current EAX value location
    movl %eax, 28(%edx)     # Save EAX to context (overwriting the pointer)
    
    # Load new CPU state from new_context
    movl 8(%esp), %eax      # Load new_context pointer into EAX
    
    # Restore segment registers first
    movw 32(%eax), %ds      # Restore DS
    movw 36(%eax), %es      # Restore ES
    movw 40(%eax), %fs      # Restore FS
    movw 44(%eax), %gs      # Restore GS
    
    # Restore stack pointer
    movl 60(%eax), %esp     # Restore ESP
    
    # Set up return context on new stack
    # Push SS, ESP, EFLAGS, CS, EIP to create interrupt return frame
    pushl 64(%eax)          # Push SS
    pushl 60(%eax)          # Push ESP 
    pushl 56(%eax)          # Push EFLAGS
    pushl 52(%eax)          # Push CS
    pushl 48(%eax)          # Push EIP
    
    # Restore general purpose registers
    movl 0(%eax), %edi      # Restore EDI
    movl 4(%eax), %esi      # Restore ESI
    movl 8(%eax), %ebp      # Restore EBP
    movl 16(%eax), %ebx     # Restore EBX
    movl 20(%eax), %edx     # Restore EDX
    movl 24(%eax), %ecx     # Restore ECX
    # Restore EAX last
    movl 28(%eax), %eax     # Restore EAX
    
    # Return to new task (this will pop EIP, CS, EFLAGS, ESP, SS)
    iret

# Alternative simpler context switch for cooperative multitasking
# void simple_context_switch(uint32_t* old_esp, uint32_t new_esp)
.global simple_context_switch
simple_context_switch:
    # Save old context
    pusha                   # Save all general purpose registers
    pushf                   # Save flags
    
    # Save old ESP
    movl 36(%esp), %eax     # Get old_esp parameter (accounting for pusha + pushf)
    movl %esp, (%eax)       # Store current ESP at old_esp location
    
    # Load new ESP
    movl 40(%esp), %esp     # Load new_esp parameter as new stack pointer
    
    # Restore new context
    popf                    # Restore flags
    popa                    # Restore all general purpose registers
    
    ret                     # Return to new task

# Helper function to initialize a new task's stack
# void setup_task_stack(uint32_t* stack_top, void* entry_point)
.global setup_task_stack
setup_task_stack:
    movl 4(%esp), %eax      # Get stack_top
    movl 8(%esp), %edx      # Get entry_point
    
    # Set up initial stack frame for new task
    # The stack should look like it was interrupted, so we push:
    # SS, ESP, EFLAGS, CS, EIP
    
    movl $0x10, -4(%eax)    # SS (kernel data segment)
    movl %eax, -8(%eax)     # ESP (stack pointer)
    movl $0x202, -12(%eax)  # EFLAGS (interrupts enabled)
    movl $0x08, -16(%eax)   # CS (kernel code segment)
    movl %edx, -20(%eax)    # EIP (entry point)
    
    # Adjust stack pointer to account for pushed values
    subl $20, %eax
    movl 4(%esp), %edx      # Get original stack_top pointer
    movl %eax, (%edx)       # Store adjusted stack pointer
    
    ret

.section .data
    # Mark stack as non-executable
    .section .note.GNU-stack,"",@progbits