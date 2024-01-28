GPPARAMS = -m32 -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = ./bin/loader.o ./bin/interruptstubs.o ./bin/gdt.o ./bin/port.o ./bin/kernel.o ./bin/interrupts.o ./bin/keyboard.o ./bin/mouse.o


%.o: %.cpp
	gcc $(GPPARAMS) -o ./bin/$@ -c $<

%.o: %.s
	as $(ASPARAMS) -o ./bin/$@ $<

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

qemu: mykernel.iso
	qemu-system-i386 -boot d -cdrom $< -m 512

.PHONY: clean
clean:
	rm -f $(objects) mykernel.bin mykernel.iso