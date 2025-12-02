# Basic language standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_language(ASM)

# Set source directory variables
set (KOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set (KOS_SRC_DIR "${KOS_ROOT_DIR}/src")
set (KOS_INCLUDE_DIR "${KOS_ROOT_DIR}/include")

# Includes
include_directories(
    ${KOS_INCLUDE_DIR}
    ${KOS_ROOT_DIR} # Allow includes like <arch/x86/...>
)

# Global compile flags mirroring the Makefile
set(COMMON_32_FLAGS "-m32 -march=i386 -mno-sse -mno-mmx -mno-sse2")
set(COMMON_NOSTDLIB_FLAGS "-nostdlib -ffreestanding -fno-builtin -fno-stack-protector")
set(CXX_NO_CXXRTTI_EXC "-fno-rtti -fno-exceptions -fno-use-cxa-atexit -fno-leading-underscore -Wno-write-strings")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS} ${CXX_NO_CXXRTTI_EXC}")
# Use -m32 (not --32) so gcc/cc assembles 32-bit
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")

# Kernel sources
set(KERNEL_ASM_SOURCES
    ${KOS_SRC_DIR}/loader.s
    ${KOS_ROOT_DIR}/arch/x86/hardware/interrupts/interruptstubs.s
)

file(GLOB_RECURSE KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/*.cpp
)

# Ensure libc is C-only: exclude any stray C++ sources under src/lib/libc
list(REMOVE_ITEM KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/lib/libc/string.cpp
    ${KOS_SRC_DIR}/lib/libc/stdint.cpp
)

# Exclude app-only C-ABI journaling shim from kernel (kernel provides its own C symbol)
list(REMOVE_ITEM KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/console/log_to_journal_c.cpp
)

# Arch-specific sources (not under src/)
set(ARCH_X86_SOURCES
    ${KOS_ROOT_DIR}/arch/x86/hardware/interrupts/interrupt_manager.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/interrupts/interrupt_handler.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/pci/peripheral_component_intercontroller.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.cpp
    # x86 I/O port access implementations (required by many drivers)
    ${KOS_ROOT_DIR}/arch/x86/hardware/port/port.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/port/port8bit.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/port/port8bitslow.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/port/port16bit.cpp
    ${KOS_ROOT_DIR}/arch/x86/hardware/port/port32bit.cpp
    # CMOS Real-Time Clock implementation (needed by logger/time_service)
    ${KOS_ROOT_DIR}/arch/x86/hardware/rtc/rtc.cpp
    # x86 GDT implementation (memory management)
    ${KOS_ROOT_DIR}/arch/x86/memory/gdt.cpp
)

# Exclude deprecated demos and commands
list(REMOVE_ITEM KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/process/pipe_demo.cpp
    ${KOS_SRC_DIR}/process/scheduler_demo.cpp
)

# Exclude deprecated/disabled service API stub (real implementation lives in service_manager.cpp)
list(REMOVE_ITEM KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/services/service_api_stub.cpp
)

# Build kernel objects into an OBJECT library
add_library(kernel_objs OBJECT
    ${KERNEL_CPP_SOURCES}
    ${KERNEL_ASM_SOURCES}
    ${ARCH_X86_SOURCES}
    ${KOS_ROOT_DIR}/src/lib/libc/string.c
    ${KOS_ROOT_DIR}/src/libc/stdio.c
)
target_compile_options(kernel_objs PRIVATE -m32 -march=i386 -mno-sse -mno-mmx -mno-sse2)
target_include_directories(kernel_objs PRIVATE ${KOS_INCLUDE_DIR} ${KOS_ROOT_DIR})

# Link kernel using ld and the provided linker script
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/mykernel.bin)

add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND ld -melf_i386 -T ${KOS_ROOT_DIR}/linker.ld -o ${KERNEL_BIN} $<TARGET_OBJECTS:kernel_objs>
    DEPENDS kernel_objs ${KOS_ROOT_DIR}/linker.ld
    VERBATIM
    COMMAND_EXPAND_LISTS
)

add_custom_target(kernel_bin ALL DEPENDS ${KERNEL_BIN})

# Also drop a copy next to sources for convenience
add_custom_command(TARGET kernel_bin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${KOS_ROOT_DIR}/mykernel.bin
)

# -----------------------
# Applications (.elf) build
# -----------------------

# Minimal support objects that apps link against
add_library(app_support_objs OBJECT
    ${KOS_ROOT_DIR}/src/console/tty.cpp
    ${KOS_ROOT_DIR}/src/console/log_to_journal.cpp
    ${KOS_ROOT_DIR}/src/console/log_to_journal_c.cpp
    ${KOS_ROOT_DIR}/src/lib/libc/string.c
    ${KOS_ROOT_DIR}/src/drivers/vga/vga.cpp
    ${KOS_ROOT_DIR}/src/libc/stdio.c
    ${KOS_ROOT_DIR}/src/libc/tokenize.c
)
target_include_directories(app_support_objs PRIVATE ${KOS_INCLUDE_DIR} ${KOS_ROOT_DIR})
target_compile_options(app_support_objs PRIVATE -m32 -march=i386 -mno-sse -mno-mmx -mno-sse2)
target_compile_definitions(app_support_objs PRIVATE KOS_BUILD_APPS=1)

function(add_app NAME)
    # Build the app object (C apps)
    add_library(${NAME}_obj OBJECT ${KOS_ROOT_DIR}/application/${NAME}.c)
    target_include_directories(${NAME}_obj PRIVATE ${KOS_INCLUDE_DIR} ${KOS_ROOT_DIR}/application)
    target_compile_options(${NAME}_obj PRIVATE -m32 -march=i386 -mno-sse -mno-mmx -mno-sse2 -ffreestanding -fno-builtin -fno-stack-protector)

    # Link into disk/bin/<name>.elf using ld (disk is now at project root)
    set(APP_ELF ${KOS_ROOT_DIR}/../disk/bin/${NAME}.elf)
    add_custom_command(
        OUTPUT ${APP_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${KOS_ROOT_DIR}/../disk/bin
        COMMAND ld -melf_i386 -e main -Ttext 0x01000000 -o ${APP_ELF} $<TARGET_OBJECTS:${NAME}_obj> $<TARGET_OBJECTS:app_support_objs>
        DEPENDS ${NAME}_obj app_support_objs
        VERBATIM
        COMMAND_EXPAND_LISTS
    )

    add_custom_target(${NAME}.elf ALL DEPENDS ${APP_ELF})
endfunction()

add_app(hello)
add_app(echo)
add_app(ls)
add_app(mkdir)
add_app(memtest)
add_app(pwd)
add_app(cd)
add_app(free)
add_app(lshw)
add_app(clear)
add_app(init)
add_app(cat)
add_app(top)
add_app(ping)
add_app(reboot)
add_app(date)
add_app(shutdown)

# Convenience aggregate
add_custom_target(apps DEPENDS hello.elf echo.elf ls.elf mkdir.elf memtest.elf pwd.elf cd.elf free.elf lshw.elf clear.elf init.elf cat.elf top.elf ping.elf reboot.elf date.elf shutdown.elf)

# Ensure apps are built before kernel
add_dependencies(kernel_bin apps)

# -----------------------
# ISO generation
# -----------------------
set(ISO_DIR ${CMAKE_BINARY_DIR}/iso)
set(ISO_BOOT_DIR ${ISO_DIR}/boot)
set(ISO_GRUB_DIR ${ISO_BOOT_DIR}/grub)
set(ISO_OUT ${CMAKE_BINARY_DIR}/mykernel.iso)

add_custom_command(
    OUTPUT ${ISO_OUT}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ISO_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_GRUB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${ISO_BOOT_DIR}/mykernel.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${KOS_ROOT_DIR}/arch/x86/boot/grub-cmake.cfg ${ISO_GRUB_DIR}/grub.cfg
    COMMAND grub-mkrescue --output=${ISO_OUT} ${ISO_DIR}
    DEPENDS kernel_bin ${KOS_ROOT_DIR}/arch/x86/boot/grub-cmake.cfg
    VERBATIM
)

add_custom_target(iso DEPENDS ${ISO_OUT})

# Optional: also copy ISO to ../output like the Makefile
add_custom_command(TARGET iso POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${KOS_ROOT_DIR}/../output
    COMMAND ${CMAKE_COMMAND} -E copy ${ISO_OUT} ${KOS_ROOT_DIR}/../output/mykernel.iso
)

# -----------------------
# QEMU helpers
# -----------------------
# Define disk image path early so qemu targets can reference it
set(DISK_IMG ${KOS_ROOT_DIR}/../disk.img)
add_custom_target(qemu
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=${DISK_IMG},format=raw,if=ide,index=0 -m 512 -smp 1 -accel tcg,thread=single -display sdl,gl=off -machine usb=off -serial mon:stdio
    DEPENDS iso diskimg
    USES_TERMINAL
)

add_custom_target(qemu-vvfat
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=fat:rw:${KOS_ROOT_DIR}/disk,format=raw,if=ide,index=0 -m 512 -smp 1 -accel tcg,thread=single -display sdl,gl=off -machine usb=off -serial mon:stdio
    DEPENDS iso
    USES_TERMINAL
)

# Read-only QEMU run (avoids write-lock on disk.img). Good for quick boot/log checks.
add_custom_target(qemu-ro
    COMMAND qemu-system-i386 -snapshot -boot d -cdrom ${ISO_OUT} -drive file=${DISK_IMG},format=raw,if=ide,index=0 -m 512 -smp 1 -accel tcg,thread=single -display sdl,gl=off -machine usb=off -serial mon:stdio
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(debug
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=fat:rw:${KOS_ROOT_DIR}/disk,format=raw,if=ide,index=0 -s -S -m 512 -smp 1 -accel tcg,thread=single -display sdl,gl=off -machine usb=off -serial mon:stdio
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(gdb
    COMMAND ${CMAKE_COMMAND} -E echo ">>> Start Qemu in Debug && GDB mode..."
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=fat:rw:${KOS_ROOT_DIR}/disk,format=raw,if=ide,index=0 -s -S -m 512 -smp 1 -accel tcg,thread=single -display sdl,gl=off -machine usb=off -serial mon:stdio &
    COMMAND sleep 1
    COMMAND gdb -x ${KOS_ROOT_DIR}/.gdbinit
    DEPENDS iso
    USES_TERMINAL
)

# -----------------------
set(DISK_IMG ${KOS_ROOT_DIR}/../disk.img)

add_custom_command(
    OUTPUT ${DISK_IMG}
    COMMAND ${CMAKE_COMMAND} -E echo "Creating FAT32 disk image at ${DISK_IMG}"
    COMMAND ${CMAKE_COMMAND} -E remove -f ${DISK_IMG}
    COMMAND dd if=/dev/zero of=${DISK_IMG} bs=1M count=128
    COMMAND mkfs.vfat -F 32 -n KOS_DISK ${DISK_IMG}
        # Prefer mtools if available; else fallback to loop-mount (requires sudo)
        COMMAND /bin/sh -c "if command -v mmd >/dev/null 2>&1 && mmd -V >/dev/null 2>&1; then \
                echo 'Using mtools to populate disk'; \
                MTOOLS_NO_VFAT=1 mmd -i ${DISK_IMG} ::/BIN || true; \
                MTOOLS_NO_VFAT=1 mmd -i ${DISK_IMG} ::/HOME || true; \
                MTOOLS_NO_VFAT=1 mmd -i ${DISK_IMG} ::/ETC || true; \
                MTOOLS_NO_VFAT=1 mcopy -i ${DISK_IMG} ${KOS_ROOT_DIR}/../disk/bin/*.elf ::/BIN || true; \
                if ls ${KOS_ROOT_DIR}/../disk/etc/*.CFG >/dev/null 2>&1; then \
                    MTOOLS_NO_VFAT=1 mcopy -i ${DISK_IMG} ${KOS_ROOT_DIR}/../disk/etc/*.CFG ::/ETC/ 2>/dev/null || true; \
                fi; \
                MTOOLS_NO_VFAT=1 mmd -i ${DISK_IMG} ::/ETC/INIT.D || true; \
                if [ -d ${KOS_ROOT_DIR}/../disk/etc/init.d ]; then \
                    MTOOLS_NO_VFAT=1 mcopy -i ${DISK_IMG} ${KOS_ROOT_DIR}/../disk/etc/init.d/* ::/ETC/INIT.D/ 2>/dev/null || true; \
                fi; \
            else \
                echo 'mtools unavailable; using loop-mount (sudo)'; \
                mkdir -p ${KOS_ROOT_DIR}/.diskmnt; \
                sudo mount -o loop ${DISK_IMG} ${KOS_ROOT_DIR}/.diskmnt; \
                sudo mkdir -p ${KOS_ROOT_DIR}/.diskmnt/BIN ${KOS_ROOT_DIR}/.diskmnt/HOME ${KOS_ROOT_DIR}/.diskmnt/ETC; \
                sudo cp ${KOS_ROOT_DIR}/../disk/bin/*.elf ${KOS_ROOT_DIR}/.diskmnt/BIN/ 2>/dev/null || true; \
                if ls ${KOS_ROOT_DIR}/../disk/etc/*.CFG >/dev/null 2>&1; then \
                    sudo cp ${KOS_ROOT_DIR}/../disk/etc/*.CFG ${KOS_ROOT_DIR}/.diskmnt/ETC/ 2>/dev/null || true; \
                fi; \
                sudo mkdir -p ${KOS_ROOT_DIR}/.diskmnt/ETC/INIT.D; \
                if [ -d ${KOS_ROOT_DIR}/../disk/etc/init.d ]; then \
                    sudo cp ${KOS_ROOT_DIR}/../disk/etc/init.d/* ${KOS_ROOT_DIR}/.diskmnt/ETC/INIT.D/ 2>/dev/null || true; \
                fi; \
                sync; \
                sudo umount ${KOS_ROOT_DIR}/.diskmnt; \
                rmdir ${KOS_ROOT_DIR}/.diskmnt; \
            fi"
    DEPENDS apps
    VERBATIM
)

add_custom_target(diskimg DEPENDS ${DISK_IMG})

# -----------------------
# Optional: Convert raw disk.img to VirtualBox VDI
# Requires: VBoxManage available in PATH
# Usage: make vdi
# -----------------------
set(DISK_VDI ${KOS_ROOT_DIR}/disk.vdi)

add_custom_command(
    OUTPUT ${DISK_VDI}
    COMMAND ${CMAKE_COMMAND} -E echo "Converting raw disk image to VDI: ${DISK_IMG} -> ${DISK_VDI}"
    COMMAND /bin/sh -c "if command -v VBoxManage >/dev/null 2>&1; then \
            VBoxManage convertfromraw ${DISK_IMG} ${DISK_VDI} --format VDI; \
        else \
            echo 'VBoxManage not found. Install VirtualBox or ensure VBoxManage is in PATH.'; \
            exit 1; \
        fi"
    DEPENDS ${DISK_IMG}
    VERBATIM
)

add_custom_target(vdi DEPENDS ${DISK_VDI})

# -----------------------
# Doxygen documentation
# -----------------------
find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${KOS_ROOT_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${KOS_ROOT_DIR}/docs/html/index.html)

    add_custom_command(
        OUTPUT ${DOXYGEN_OUT}
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${KOS_ROOT_DIR}
        COMMENT "Generating API documentation with Doxygen"
        DEPENDS ${DOXYGEN_IN}
        VERBATIM
    )

    add_custom_target(docs DEPENDS ${DOXYGEN_OUT})

    add_custom_target(docs-clean
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${KOS_ROOT_DIR}/docs
        COMMENT "Cleaning Doxygen documentation"
    )
else()
    message(STATUS "Doxygen not found. 'docs' target will be unavailable.")
endif()