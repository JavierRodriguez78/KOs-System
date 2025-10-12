GPPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = ../obj/loader.o  \
          ../obj/drivers/driver.o \
		  ../obj/hardware/interruptstubs.o \
		  ../obj/memory/gdt.o \
		  ../obj/hardware/port.o \
		  ../obj/kernel.o \
		  ../obj/hardware/interrupts.o \
		  ../obj/drivers/keyboard.o \
		  ../obj/drivers/mouse.o \
		  ../obj/drivers/vga.o \
		  ../obj/drivers/ata.o \
		  ../obj/fs/filesystem.o \
		  ../obj/fs/fat16.o \
		  ../obj/fs/fat32.o \
		  ../obj/hardware/pci.o \
		  ../obj/hardware/rtc.o \
		  ../obj/console/tty.o  \
		  ../obj/console/logger.o \
		  ../obj/console/shell.o \
		  ../obj/console/logo.o \
		  ../obj/console/shellKeyboardHandler.o \
		  ../obj/console/registry.o \
		  ../obj/graphics/framebuffer.o \
		  ../obj/lib/libc/string.o \
		  ../obj/lib/elfloader.o \
		  ../obj/lib/sysapi.o \
		  ../obj/lib/newdelete.o \
		  ../obj/lib/string.o  \
		  ../obj/lib/stdio.o

# Applications to build into disk/bin
APPS = hello echo ls memtest mkdir
APP_OBJDIR = ../obj/application
APP_BINDIR = disk/bin
APP_OBJS = $(patsubst %, $(APP_OBJDIR)/%.o, $(APPS))
APP_BINS = $(patsubst %, $(APP_BINDIR)/%.elf, $(APPS))


../obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GPPARAMS) -o $@ -c $<

../obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o  $@ $<

# Build generic C sources under src/ (for C support files like libc/api.c)
../obj/%.o: src/%.c
	mkdir -p $(@D)
	gcc $(CPARAMS) -o $@ -c $<


mykernel.bin: linker.ld $(objects)
	ld $(LDPARAMS) -T $< -o $@ $(objects)

# Build application objects from application/*.c (C apps)
CPARAMS = -m32 -Iinclude -nostdlib -ffreestanding -fno-builtin -fno-stack-protector -fno-leading-underscore -Wno-write-strings
$(APP_OBJDIR)/%.o: application/%.c
	mkdir -p $(@D)
	gcc $(CPARAMS) -o $@ -c $<

# Link applications as simple ELF32 with entry at symbol 'main'
$(APP_BINDIR)/%.elf: $(APP_OBJDIR)/%.o ../obj/console/tty.o ../obj/lib/libc/string.o ../obj/lib/newdelete.o ../obj/drivers/vga.o
	mkdir -p $(APP_BINDIR)
	ld -melf_i386 -e main -Ttext 0x01000000 -o $@ $^
	@echo "Built app $@"

# Also generate explicit rules per app (robust against pattern-rule edge cases)
define MAKE_APP
$(APP_BINDIR)/$(1).elf: $(APP_OBJDIR)/$(1).o ../obj/console/tty.o ../obj/lib/libc/string.o ../obj/lib/newdelete.o ../obj/drivers/vga.o
	mkdir -p $(APP_BINDIR)
	ld -melf_i386 -e main -Ttext 0x01000000 -o $$@ $$^
	@echo "Built app $$@ (explicit)"
endef
$(foreach app,$(APPS),$(eval $(call MAKE_APP,$(app))))

.PHONY: apps
apps: $(APP_BINS)

# Ensure apps are built when building the kernel binary
mykernel.bin: apps

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
	# Runs kernel from CD-ROM and attaches a primary master HDD image at ./disk.img
	# Create and format ./disk.img as FAT32 before running this target
	qemu-system-i386 -boot d -cdrom $< -hda ./disk.img -m 512

qemu-vvfat: ../output/mykernel.iso
	# Maps ./disk directory as a FAT drive (vvfat) on primary master. Create ./disk and put files there.
	qemu-system-i386 -boot d -cdrom $< -drive file=fat:rw:./disk,format=raw,if=ide,index=0 -m 512

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
	rm -rf $(APP_BINDIR)/*.elf

# -----------------------
# Documentation (Doxygen)
# -----------------------
.PHONY: docs docs-clean
docs:
	@which doxygen >/dev/null 2>&1 || { echo "Error: doxygen not found. Install it (e.g., sudo apt-get install doxygen graphviz)"; exit 1; }
	doxygen Doxyfile
	@echo "HTML docs generated in ./docs/html/index.html"

docs-clean:
	rm -rf docs