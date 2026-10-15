# minos - my little x86-64 kernel

CC      := gcc
LD      := ld
QEMU    := qemu-system-x86_64
BUILD   := build

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wextra \
           -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
           -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
           -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mcmodel=kernel \
           -Iinclude -Ikernel
ASFLAGS := -g -Iinclude -Ikernel
LDFLAGS := -nostdlib -n --no-warn-rwx-segments -z max-page-size=0x1000 -T kernel/linker.ld

KSRCS_C := $(shell find kernel -name '*.c')
KSRCS_S := $(shell find kernel -name '*.S')
KOBJS   := $(patsubst %.c,$(BUILD)/%.o,$(KSRCS_C)) $(patsubst %.S,$(BUILD)/%.o,$(KSRCS_S))

KERNEL  := $(BUILD)/kernel.elf
ISO     := $(BUILD)/minos.iso

QEMUFLAGS := -m 256M -no-reboot -no-shutdown

.PHONY: all iso run run-nox debug clean

all: $(KERNEL)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

$(KERNEL): $(KOBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJS)

iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	@mkdir -p $(BUILD)/isodir/boot/grub
	cp $(KERNEL) $(BUILD)/isodir/boot/kernel.elf
	cp grub.cfg $(BUILD)/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/isodir 2>/dev/null

# opens a qemu window, serial output shows up in this terminal
run: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO) -serial stdio

# no window, just serial (ctrl-a x to quit)
run-nox: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO) -serial mon:stdio -display none

# waits for gdb: gdb build/kernel.elf -ex 'target remote :1234'
debug: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO) -serial stdio -s -S

clean:
	rm -rf $(BUILD)

-include $(KOBJS:.o=.d)
