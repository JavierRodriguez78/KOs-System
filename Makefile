GPPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = ./obj/loader.o  \
          ./obj/drivers/driver.o \
		  ./obj/hardware/interruptstubs.o \
		  ./obj/gdt.o \
		  ./obj/hardware/port.o \
		  ./obj/kernel.o \
		  ./obj/hardware/interrupts.o \
		  ./obj/drivers/keyboard.o \
		  ./obj/drivers/mouse.o \
		  ./obj/hardware/pci.o


obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GPPARAMS) -o $@ -c $<

obj/%.o: src/%.s
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

qemu: mykernel.iso
	qemu-system-i386 -boot d -cdrom $< -m 512

.PHONY: clean
clean:
	rm -rf obj mykernel.bin mykernel.iso