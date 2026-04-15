# Kos-system

KOs-system is a hobby operating system developed for the 64-bit platform using C++ and assembler. The project is currently in a very early stage of development and is not yet ready for use. The project is being developed as a learning experience and is not intended to be used as a production operating system.

## Screenshots

<img width="400" alt="400" src="https://github.com/JavierRodriguez78/KOs-System/blob/main/docs/media/kos-system.png">


## Getting Started 🚀

This is how to build KO operating system from source. 

### Prerequisites 📋

This is the list of required packages to build the operating system from source. (Note the build scripts should install these automatically):

- Make
- Build-essential
- gdb
- qemu-system-i386
- nasm
- grub-common
- grub-pc-bin
- xorriso

```bash
sudo apt update
sudo apt install -y make build-essential  gdb qemu-system-i386 nasm grub-common grub-pc-bin xorriso
```
### Installation 🔧

1. Clone the repo

```bash
git clone https://github.com/JavierRodriguez78/KOs-System.git
cd KOs-System
```

2. Create Bin

```bash
cd kernel
make mykernel.bin
```

3. Create Iso

```bash
make mykernel.iso
```

4. Execute Qemu
```bash
make qemu
```

5. Execute Debug Qemu
```bash
make gdb
``` 

## Build with 🛠️

- [c++ ](https://isocpp.org/) - c17
- [nasm](https://nasm.us/) - Assembler


## Contributing 🖇️

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are greatly appreciated.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement". Don't forget to give the project a star! Thanks again!

Fork the Project
Create your Feature Branch (git checkout -b feature/AmazingFeature)
Commit your Changes (git commit -m 'Add some AmazingFeature')
Push to the Branch (git push origin feature/AmazingFeature)
Open a Pull Request

## Input And Login Notes

Current behavior after recent stability fixes:

- Text mode keyboard input is interrupt-driven (IRQ1) and does not use keyboard polling.
- Graphics mode login also receives keyboard input from IRQ1 and updates UI through the Window Manager tick loop.
- Service ticking in graphics mode is driven by the main kernel loop to keep UI repaint deterministic.
- Deferred shell startup is used in graphics mode: the shell starts after successful login.

Why this matters:

- Avoids duplicate key delivery caused by mixing polling and IRQ reads.
- Avoids startup stalls caused by services touching framebuffer directly after compositor/window manager initialization.
- Keeps text mode input path stable while allowing graphics mode login and UI interaction.

## Input Debug Toggle (KOS_INPUT_DEBUG)

Input diagnostics are controlled with a centralized build flag.

- Enable verbose input debug logs:

```bash
cmake -S . -B build -DKOS_INPUT_DEBUG=ON
cmake --build build -j$(nproc)
cmake --build build --target iso -j$(nproc)
```

- Disable verbose input debug logs (recommended for normal/prod runs):

```bash
cmake -S . -B build -DKOS_INPUT_DEBUG=OFF
cmake --build build -j$(nproc)
cmake --build build --target iso -j$(nproc)
```

- Alternative with presets (single configure/build command):

```bash
# Debug logs ON
cmake --preset debug-input-on
cmake --build --preset debug-input-on-iso

# Debug logs OFF
cmake --preset debug-input-off
cmake --build --preset debug-input-off-iso
```


## Roadmap

* Bootloader
* GDT
* IDT
* Interrupts
* Keyboard
* Mouse
* PCI
* ATA
* VESA
* Multitasking
* GUI

## License 📄

Distributed under the BSD 3-Clause License. See LICENSE for more information.

⌨️ with ❤️ por [Xavi Rodriguez](https://github.com/JavierRodriguez78) 😊
