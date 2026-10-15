# minos-kernel

minos is a small 64-bit operating system kernel I'm writing from scratch in C and x86 assembly. It boots with GRUB and runs inside the QEMU emulator.

The goal is a kernel that can run its own user programs: virtual memory, a page allocator, a scheduler, system calls and a little shell.

## Progress

- [x] Boot with GRUB (multiboot2) and switch the CPU from 32 bit to 64 bit mode
- [x] Run the kernel in the higher half of memory (`0xFFFFFFFF80000000`)
- [x] Print text to the serial port and the VGA screen (`kprintf`)
- [x] Read the memory map GRUB gives us
- [ ] Interrupts, timer and keyboard (GDT, IDT, PIC, PIT)
- [ ] Physical page allocator
- [ ] Virtual memory (4 level paging)
- [ ] Processes and a round robin scheduler
- [ ] System calls
- [ ] User programs and a shell

## Getting started

You need Linux (WSL works fine) with gcc, make, QEMU and the GRUB tools. On Ubuntu:

```
sudo apt install build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

Then from the project folder:

| Command        | What it does                                              |
|----------------|-----------------------------------------------------------|
| `make`         | builds the kernel (`build/kernel.elf`)                    |
| `make run`     | builds a bootable iso and starts it in a QEMU window      |
| `make run-nox` | same but no window, output in the terminal (ctrl-a x quits) |
| `make debug`   | starts QEMU paused so you can attach gdb on port 1234     |
| `make clean`   | deletes the build folder                                  |

If it worked you'll see something like:

```
minos booting...
hello from 64 bit long mode! kmain is at 0xffffffff801010a0
bootloader: GRUB 2.12
memory map:
  0000000000000000 - 000000000009fbff  usable
  ...
usable ram: 255 MB
```

## How booting works

1. GRUB loads `kernel.elf` at 1MB and jumps to `_start` in `kernel/boot.S`. The CPU is still in 32 bit mode here.
2. `boot.S` makes some temporary page tables. They map the first 1GB of RAM twice, once at address 0 and once up at `0xFFFFFFFF80000000`.
3. It turns on paging and long mode, then jumps up to the higher half.
4. `kmain` in `kernel/main.c` sets up the serial port and screen and prints stuff.

## Project layout

```
kernel/
  boot.S         first code that runs, gets us into 64 bit mode
  linker.ld      where everything goes in memory
  main.c         kmain, the C entry point
  kernel.h       shared helpers (kprintf, port io, etc)
  multiboot2.h   structs for reading GRUB's boot info
  dev/           serial port and VGA text screen
  lib/           printf and string functions
grub.cfg         GRUB menu entry
Makefile
```
