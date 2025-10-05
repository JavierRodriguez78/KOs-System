GPPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = ../obj/loader.o  \
          ../obj/drivers/driver.o \
		  ../obj/hardware/interruptstubs.o \
		  ../obj/gdt.o \
		  ../obj/hardware/port.o \
		  ../obj/kernel.o \
		  ../obj/hardware/interrupts.o \
		  ../obj/drivers/keyboard.o \
		  ../obj/drivers/mouse.o \
		  ../obj/drivers/vga.o \
		  ../obj/hardware/pci.o \
		  ../obj/console/tty.o  \
		  ../obj/console/shell.o \
		  ../obj/lib/libc.o


../obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GPPARAMS) -o $@ -c $<

../obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o  $@ $<

mykernel.bin: linker.ld $(objects)
	ld $(LDPARAMS) -T $< -o $@ $(objects)

mykernel.iso: mykernel.bin
	mkdir ./iso
	mkdir ./iso/boot
	mkdir ./iso/boot/grub
	cp mykernel.bin ./iso/boot/mykernel.bin
	echo 'set timeout=0' 						>> ./iso/boot/grub/grub.cfg
	echo 'set default=0' 						>> ./iso/boot/grub/grub.cfg
	echo '' 									>> ./iso/boot/grub/grub.cfg
	echo 'menuentry "KOs Operating System" {'	>> ./iso/boot/grub/grub.cfg
	echo ' multiboot /boot/mykernel.bin' 	    	>> ./iso/boot/grub/grub.cfg
	echo ' boot' 								>> ./iso/boot/grub/grub.cfg
	echo '}' >> ./iso/boot/grub/grub.cfg
	grub-mkrescue --output=mykernel.iso ./iso
	rm -rf ./iso
	mkdir ../output
	mv ./mykernel.iso ../output/mykernel.iso

qemu: ../output/mykernel.iso
	qemu-system-i386 -boot d -cdrom $< -m 512

debug: ../output/mykernel.iso
	qemu-system-i386 -boot d -cdrom $< -s -S -m 512

gdb: ../output/mykernel.iso
	@echo ">>> Start Qemu in Debug && GDB mode..."
	@qemu-system-i386 -boot d -cdrom $< -s -S & 
	sleep 1
	gdb -x .gdbinit

.PHONY: clean
clean:
	rm -rf ../obj mykernel.bin mykernel.iso ../output