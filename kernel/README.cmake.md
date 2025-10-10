# CMake build (experimental)

This adds a CMake-based build that mirrors the original Makefile.

## Prerequisites
- gcc, ld, as that can produce 32-bit code (i386). On 64-bit hosts you may need multilib:
  - Debian/Ubuntu: `sudo apt-get install build-essential gcc-multilib g++-multilib binutils grub-pc-bin xorriso qemu-system-i386`
- CMake 3.16+

## Configure and build
```
mkdir -p build && cd build
cmake ..
cmake --build . -j
```
Artifacts:
- `mykernel.bin` (also copied to source dir)
- apps in `disk/bin/*.elf`

## Create ISO
```
cmake --build . --target iso
```
ISO will be at `build/mykernel.iso` and copied to `../output/mykernel.iso` relative to the kernel folder.

## Run in QEMU
```
cmake --build . --target qemu      # requires ./disk.img (real HDD image)
# or
cmake --build . --target qemu-vvfat  # maps ./disk folder as drive
```

## Notes
- Linking is done directly with `ld` to respect the custom script `linker.ld`.
- ASM files are assembled in 32-bit mode.
- If your host toolchain prefixes (e.g., `i686-elf-`), adjust commands in `CMakeLists.txt` accordingly.
