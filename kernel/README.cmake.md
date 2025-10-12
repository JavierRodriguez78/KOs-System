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

## Create dis image
```
cmake --build . --target diskimg
```

## Run in QEMU

Install:
sudo apt-get install dosfstools mtools qemu-system-i386


```
cmake --build . --target qemu      # requires ./disk.img (real HDD image)
# or
cmake --build . --target qemu-vvfat  # maps ./disk folder as drive
```

### vvfat notes
- The qemu-vvfat target maps the host directory `kernel/disk/` as a writable FAT drive. Files placed in `disk/bin/` (like `mkdir.elf`) are visible inside KOS as `/bin/...` when using the FAT32 loader.
- Changes you make inside the VM will reflect on the host folder because vvfat writes through to the host.

### mkdir command
- A `mkdir` app is provided: `mkdir [-p] [-h] <dir> [dir2 ...]`.
- Current kernel FS is read-only; the syscall is stubbed and prints a hint message until write support is implemented.

## Execute Docs
* Ubuntu/Debina packages:
sudo apt-get update
sudo apt-get install doxygen graphviz
Execute:
```
cmake --build build --target docs
```

## Notes
- Linking is done directly with `ld` to respect the custom script `linker.ld`.
- ASM files are assembled in 32-bit mode.
- If your host toolchain prefixes (e.g., `i686-elf-`), adjust commands in `CMakeLists.txt` accordingly.
