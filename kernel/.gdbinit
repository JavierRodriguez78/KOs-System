# =============================
# KOs-System .gdbinit
# =============================


# Connect to Qemu in debug mode
target remote localhost:1234

# Load Kernel symbols
symbol-file mykernel.bin

# Breakpoints
# Init point to kernel
break _start

# Optional: Breakpoint to init kernel
break kernelMain

# Optional: debugger into keyboard.
break kos::drivers::KeyboardDriver::HandleInterrupt

# Show register to start
layout regs