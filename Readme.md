# Kos-system

KOs-system is a hobby operating system developed for the 64-bit platform using C++ and assembler. The project is currently in a very early stage of development and is not yet ready for use. The project is being developed as a learning experience and is not intended to be used as a production operating system.

## Screenshots



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

## Build with 🛠️

Explica qué tecnologías usaste para construir este proyecto. Aquí algunos ejemplos:

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

## Wiki 📖

Puedes encontrar mucho más sobre cómo usar este proyecto en nuestra [Wiki](https://github.com/your/project/wiki)

## Soporte

Si tienes algún problema o sugerencia, por favor abre un problema [aquí](https://github.com/your/project/issues).

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