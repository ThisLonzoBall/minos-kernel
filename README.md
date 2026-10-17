# minos-kernel

minos is a small 64-bit operating system kernel I'm writing from scratch in C and x86 assembly. It boots with GRUB and runs inside the QEMU emulator.

The goal is a kernel that can run its own user programs: virtual memory, a page allocator, a scheduler, system calls and a little shell.

## Progress

- [x] Boot with GRUB (multiboot2) and switch the CPU from 32 bit to 64 bit mode
- [x] Run the kernel in the higher half of memory (`0xFFFFFFFF80000000`)
- [x] Print text to the serial port and the VGA screen (`kprintf`)
- [x] Read the memory map GRUB gives us
- [x] GDT and TSS (kernel/user segments, separate stack for double faults)
- [x] IDT with handlers for all 256 vectors, CPU exceptions print the registers
- [x] PIC remapped, timer ticking at 100Hz (PIT)
- [x] PS/2 keyboard driver (shift, caps lock, ctrl)
- [x] Physical page allocator (bitmap, up to 4GB)
- [x] Page table functions (map, unmap, translate) and all of RAM mapped at `0xFFFF800000000000`
- [ ] Kernel heap (kmalloc) and separate address spaces for processes
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
gdt + tss loaded
idt loaded
pmm: 252 MB free (64xxx pages), kernel ends at ...
vmm: mapped 256 MB of ram at ffff800000000000, identity map removed
pmm: self test ok (first page was ...)
vmm: self test ok
interrupts on
timer works, 100 ticks went by in about 1 second
type something (keyboard or serial):
>
```

Anything you type in the QEMU window (or in the terminal for `make run-nox`) gets echoed back.

To check that exceptions work, add `ud2test` to the end of the `multiboot2` line in `grub.cfg`. The kernel will run a bad instruction on purpose and you should see an "invalid opcode" dump with all the registers.

## How booting works

1. GRUB loads `kernel.elf` at 1MB and jumps to `_start` in `kernel/boot.S`. The CPU is still in 32 bit mode here.
2. `boot.S` makes some temporary page tables. They map the first 1GB of RAM twice, once at address 0 and once up at `0xFFFFFFFF80000000`.
3. It turns on paging and long mode, then jumps up to the higher half.
4. `kmain` in `kernel/main.c` sets up the serial port and screen and prints stuff.
5. Later the vmm maps all of RAM at `0xFFFF800000000000` and throws away the low identity map.

## Memory layout

| Virtual address        | What's there                              |
|------------------------|-------------------------------------------|
| `0x0000000000000000`   | nothing yet (user programs will go here)  |
| `0xFFFF800000000000`   | all of physical RAM (the "direct map")    |
| `0xFFFFFFFF80000000`   | the kernel (first 1GB of RAM mapped here) |

## Project layout

```
kernel/
  boot.S         first code that runs, gets us into 64 bit mode
  linker.ld      where everything goes in memory
  main.c         kmain, the C entry point
  kernel.h       shared helpers (kprintf, port io, etc)
  multiboot2.h   structs for reading GRUB's boot info
  arch/          GDT, IDT, interrupt stubs, PIC and timer
  dev/           serial port, VGA text screen, keyboard
  mm/            memory management (page allocator, page tables)
  lib/           printf and string functions
grub.cfg         GRUB menu entry
Makefile
```
