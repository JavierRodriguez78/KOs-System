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

## Graphical terminal

When booted with the framebuffer (graphics) entry, the Window Manager now creates a "Terminal" window at startup and mirrors all TTY output into it. The classic text-mode shell continues to run, but its output/input are visible and interactive inside this window:

- Output: Anything written via `TTY::Write/PutChar` is also drawn using a tiny 8x8 bitmap font inside the terminal window.
- Input: Keyboard events are still delivered to the shell through the existing keyboard handler; echoed characters appear in the terminal.

Tips:
- The window can be dragged by its title bar; z-order is handled by click-to-focus.
- If you boot without framebuffer, the system behaves as before in text mode.
 - Keyboard input now respects window focus: click inside the Terminal window (anywhere on it) to give it focus; only the focused window receives shell input.

### Terminal usability updates
- Focus highlight: the focused window draws an accent bar below the title bar.
- Readability: the terminal defaults to an 8x16 font for taller, clearer text.
- Scrollback: use PageUp/PageDown to navigate history in the terminal window.
  - PageUp/PageDown are handled from the keyboard driver via extended scancodes (E0 49 / E0 51).
  - While scrolled back, the cursor is rendered only when visible in the current viewport.
