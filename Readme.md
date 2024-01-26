//Se require
Make
build-essential
gdb
qemu-system-i386
nasm
grub-common
grub-pc-bin
xorriso







// Creación de los OBJ:
make kernel.o
make loader.o

//Creación del BIN:
make mykernel.bin

//Ejecución en QEMU
make qemu