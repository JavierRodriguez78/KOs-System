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
)

# Global compile flags mirroring the Makefile
set(COMMON_32_FLAGS "-m32")
set(COMMON_NOSTDLIB_FLAGS "-nostdlib -ffreestanding -fno-builtin -fno-stack-protector")
set(CXX_NO_CXXRTTI_EXC "-fno-rtti -fno-exceptions -fno-use-cxa-atexit -fno-leading-underscore -Wno-write-strings")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_32_FLAGS} ${COMMON_NOSTDLIB_FLAGS} ${CXX_NO_CXXRTTI_EXC}")
# Use -m32 (not --32) so gcc/cc assembles 32-bit
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")

# Kernel sources
set(KERNEL_ASM_SOURCES
    ${KOS_SRC_DIR}/loader.s
    ${KOS_SRC_DIR}/hardware/interruptstubs.s
)

file(GLOB_RECURSE KERNEL_CPP_SOURCES
    ${KOS_SRC_DIR}/*.cpp
)

# Build kernel objects into an OBJECT library
add_library(kernel_objs OBJECT
    ${KERNEL_CPP_SOURCES}
    ${KERNEL_ASM_SOURCES}
)
target_compile_options(kernel_objs PRIVATE -m32)
target_include_directories(kernel_objs PRIVATE ${KOS_INCLUDE_DIR})

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
    ${KOS_ROOT_DIR}/src/lib/libc/string.cpp
    ${KOS_ROOT_DIR}/src/lib/newdelete.cpp
    ${KOS_ROOT_DIR}/src/drivers/vga.cpp
)
target_include_directories(app_support_objs PRIVATE ${KOS_INCLUDE_DIR})
target_compile_options(app_support_objs PRIVATE -m32)

function(add_app NAME)
    # Build the app object (C apps)
    add_library(${NAME}_obj OBJECT ${KOS_ROOT_DIR}/application/${NAME}.c)
    target_include_directories(${NAME}_obj PRIVATE ${KOS_INCLUDE_DIR} ${KOS_ROOT_DIR}/application)
    target_compile_options(${NAME}_obj PRIVATE -m32 -ffreestanding -fno-builtin -fno-stack-protector)

    # Link into disk/bin/<name>.elf using ld
    set(APP_ELF ${KOS_ROOT_DIR}/disk/bin/${NAME}.elf)
    add_custom_command(
        OUTPUT ${APP_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${KOS_ROOT_DIR}/disk/bin
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

# Convenience aggregate
add_custom_target(apps DEPENDS hello.elf echo.elf ls.elf mkdir.elf memtest.elf pwd.elf cd.elf)

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
    COMMAND ${CMAKE_COMMAND} -E copy ${KOS_ROOT_DIR}/boot/grub-cmake.cfg ${ISO_GRUB_DIR}/grub.cfg
    COMMAND grub-mkrescue --output=${ISO_OUT} ${ISO_DIR}
    DEPENDS kernel_bin ${KOS_ROOT_DIR}/boot/grub-cmake.cfg
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
add_custom_target(qemu
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -hda ${KOS_ROOT_DIR}/disk.img -m 512
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(qemu-vvfat
    COMMAND qemu-system-i386 -boot d -cdrom ${ISO_OUT} -drive file=fat:rw:${KOS_ROOT_DIR}/disk,format=raw,if=ide,index=0 -m 512
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
    COMMAND gdb -x ${KOS_ROOT_DIR}/.gdbinit
    DEPENDS iso
    USES_TERMINAL
)

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