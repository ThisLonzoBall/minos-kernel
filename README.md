# minos

A small x86-64 kernel I'm building from scratch. It boots with GRUB (multiboot2) and runs in QEMU.

## What works so far

- Boots through GRUB, switches from 32 bit to 64 bit long mode
- Kernel lives in the higher half (0xFFFFFFFF80000000)
- Prints to the serial port and the VGA text screen
- Reads the memory map that GRUB gives us

## Coming next

- GDT/IDT, interrupts, timer, keyboard
- Physical page allocator and virtual memory
- Processes and a scheduler
- System calls and userspace programs (with a little shell)

## Building

You need gcc, make, QEMU and GRUB tools. On Ubuntu/WSL:

```
sudo apt install build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

Then:

```
make          # just builds build/kernel.elf
make run      # builds the iso and boots it in qemu
make run-nox  # same thing but serial only, no window (ctrl-a x to quit)
```
