#include "arch/arch.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
extern uint64_t isr_table[256];

static const char *exception_names[32] = {
    "divide by zero", "debug", "nmi", "breakpoint", "overflow",
    "bound range", "invalid opcode", "device not available", "double fault",
    "coprocessor overrun", "invalid tss", "segment not present",
    "stack segment fault", "general protection fault", "page fault",
    "reserved", "x87 fpu error", "alignment check", "machine check",
    "simd error", "virtualization", "control protection",
    "reserved", "reserved", "reserved", "reserved", "reserved", "reserved",
    "hypervisor injection", "vmm communication", "security", "reserved",
};

static void set_gate(int n, uint64_t handler, uint8_t flags, uint8_t ist)
{
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = KERNEL_CS;
    idt[n].ist = ist;
    idt[n].flags = flags;
    idt[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = handler >> 32;
    idt[n].zero = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 256; i++)
        set_gate(i, isr_table[i], 0x8E, 0);   // present, ring 0, interrupt gate

    // double fault gets its own stack (ist1) so a busted stack doesn't triple fault us
    set_gate(8, isr_table[8], 0x8E, 1);

    struct idt_ptr ptr = { sizeof(idt) - 1, (uint64_t)idt };
    asm volatile("lidt %0" : : "m"(ptr));
}

static void dump_frame(struct trap_frame *tf)
{
    kprintf("  rip=%016lx cs=%lx rflags=%lx\n", tf->rip, tf->cs, tf->rflags);
    kprintf("  rsp=%016lx ss=%lx err=%lx\n", tf->rsp, tf->ss, tf->err);
    kprintf("  rax=%016lx rbx=%016lx rcx=%016lx\n", tf->rax, tf->rbx, tf->rcx);
    kprintf("  rdx=%016lx rsi=%016lx rdi=%016lx\n", tf->rdx, tf->rsi, tf->rdi);
    kprintf("  rbp=%016lx r8 =%016lx r9 =%016lx\n", tf->rbp, tf->r8, tf->r9);
    kprintf("  cr2=%016lx cr3=%016lx\n", read_cr2(), read_cr3());
}

static void handle_exception(struct trap_frame *tf)
{
    kprintf("\nexception %lu: %s\n", tf->vector, exception_names[tf->vector]);
    if (tf->vector == 14) {
        kprintf("  page fault at %016lx (%s, %s, %s)\n", read_cr2(),
                (tf->err & 1) ? "protection" : "not present",
                (tf->err & 2) ? "write" : "read",
                (tf->err & 4) ? "user" : "kernel");
    }
    dump_frame(tf);
    // later on: if this came from user mode just kill the process
    panic("unhandled exception in kernel");
}

void interrupt_handler(struct trap_frame *tf)
{
    if (tf->vector < 32) {
        handle_exception(tf);
    } else if (tf->vector < IRQ_BASE + 16) {
        extern void irq_dispatch(int irq, struct trap_frame *tf);
        irq_dispatch(tf->vector - IRQ_BASE, tf);
    } else {
        kprintf("got weird interrupt %lu, ignoring\n", tf->vector);
    }
}
