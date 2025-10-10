cmake_minimum_required(VERSION 3.16)

project(KOSKernel LANGUAGES C CXX ASM)

# Basic language standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_language(ASM)

# Includes
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Global compile flags mirroring the Makefile
set(COMMON_32_FLAGS "-m32")
set(COMMON_NOSTDLIB_FLAGS "-nostdlib -ffreestanding -fno-builtin -fno-stack-protector")
set(CXX_NO_CXXRTTI_EXC "-fno-rtti -fno-exceptions -fno-use-cxa-atexit -fno-leading-underscore -Wno-write-strings")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS} ${CXX_NO_CXXRTTI_EXC}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --32")

# Kernel sources
set(KERNEL_ASM_SOURCES
    ${CMAKE_SOURCE_DIR}/src/loader.s
    ${CMAKE_SOURCE_DIR}/src/hardware/interruptstubs.s
)

file(GLOB_RECURSE KERNEL_CPP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Build kernel objects into an OBJECT library
add_library(kernel_objs OBJECT
    ${KERNEL_CPP_SOURCES}
    ${KERNEL_ASM_SOURCES}
)
target_compile_options(kernel_objs PRIVATE -m32)
target_include_directories(kernel_objs PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link kernel using ld and the provided linker script
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/mykernel.bin)

add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND ld -melf_i386 -T ${CMAKE_SOURCE_DIR}/linker.ld -o ${KERNEL_BIN} $<TARGET_OBJECTS:kernel_objs>
    DEPENDS kernel_objs ${CMAKE_SOURCE_DIR}/linker.ld
    VERBATIM
)

add_custom_target(kernel_bin ALL DEPENDS ${KERNEL_BIN})

# Also drop a copy next to sources for convenience (parity with Makefile)
add_custom_command(TARGET kernel_bin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${CMAKE_SOURCE_DIR}/mykernel.bin
)

# -----------------------
# Applications (.elf) build
# -----------------------

# Minimal support objects that apps link against (parity with Makefile)
add_library(app_support_objs OBJECT
    ${CMAKE_SOURCE_DIR}/src/console/tty.cpp
    ${CMAKE_SOURCE_DIR}/src/lib/libc/string.cpp
    ${CMAKE_SOURCE_DIR}/src/lib/newdelete.cpp
    ${CMAKE_SOURCE_DIR}/src/drivers/vga.cpp
)
target_include_directories(app_support_objs PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(app_support_objs PRIVATE -m32)

function(add_app NAME)
    # Build the app object (C apps)
    add_library(${NAME}_obj OBJECT ${CMAKE_SOURCE_DIR}/application/${NAME}.c)
    target_include_directories(${NAME}_obj PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/application)
    target_compile_options(${NAME}_obj PRIVATE -m32 -ffreestanding -fno-builtin -fno-stack-protector)

    # Link into disk/bin/<name>.elf using ld
    set(APP_ELF ${CMAKE_SOURCE_DIR}/disk/bin/${NAME}.elf)
    add_custom_command(
        OUTPUT ${APP_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/disk/bin
        COMMAND ld -melf_i386 -e main -Ttext 0x01000000 -o ${APP_ELF} $<TARGET_OBJECTS:${NAME}_obj> $<TARGET_OBJECTS:app_support_objs>
        DEPENDS ${NAME}_obj app_support_objs
        VERBATIM
    )

    add_custom_target(${NAME}.elf ALL DEPENDS ${APP_ELF})
endfunction()

add_app(hello)
add_app(echo)
add_app(ls)

# Convenience aggregate like the Makefile
add_custom_target(apps DEPENDS hello.elf echo.elf ls.elf)

# Ensure apps are built before kernel (parity with Makefile rule)
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
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/grub-cmake.cfg ${ISO_GRUB_DIR}/grub.cfg
    COMMAND grub-mkrescue --output=${ISO_OUT} ${ISO_DIR}
    DEPENDS kernel_bin ${CMAKE_SOURCE_DIR}/boot/grub-cmake.cfg
    VERBATIM
)

add_custom_target(iso DEPENDS ${ISO_OUT})

# Optional: also copy ISO to ../output like the Makefile
add_custom_command(TARGET iso POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/../output
    COMMAND ${CMAKE_COMMAND} -E copy ${ISO_OUT} ${CMAKE_SOURCE_DIR}/../output/mykernel.iso
)

# -----------------------
# QEMU helpers
# -----------------------
add_custom_target(qemu
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -hda ${CMAKE_SOURCE_DIR}/disk.img -m 512
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(qemu-vvfat
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=fat:rw:${CMAKE_SOURCE_DIR}/disk,format=raw,if=ide,index=0 -m 512
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(debug
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -s -S -m 512
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(gdb
    COMMAND ${CMAKE_COMMAND} -E echo ">>> Start Qemu in Debug && GDB mode..."
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -s -S &
    COMMAND sleep 1
    COMMAND gdb -x ${CMAKE_SOURCE_DIR}/.gdbinit
    DEPENDS iso
    USES_TERMINAL
)
