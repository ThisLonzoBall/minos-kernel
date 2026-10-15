#ifndef ARCH_H
#define ARCH_H

#include "kernel.h"

// segment selectors (see gdt.c)
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   (0x18 | 3)
#define USER_CS   (0x20 | 3)
#define TSS_SEL   0x28

// what the cpu + isr.S push on the stack when an interrupt happens.
// order has to match isr.S exactly
struct trap_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, err;
    // cpu pushes these
    uint64_t rip, cs, rflags, rsp, ss;
};

// gdt.c
void gdt_init(void);
void tss_set_kernel_stack(uint64_t rsp0);

// idt.c
void idt_init(void);

// pic.c
#define IRQ_BASE  32
#define IRQ_TIMER 0
#define IRQ_KBD   1
typedef void (*irq_handler_t)(struct trap_frame *tf);
void pic_init(void);
void pic_eoi(int irq);
void pic_unmask(int irq);
void irq_register(int irq, irq_handler_t fn);

// pit.c
#define TIMER_HZ 100
void pit_init(void);
uint64_t timer_ticks(void);

// kbd.c
void kbd_init(void);
int kbd_getc(void);   // -1 if nothing typed

static inline uint64_t read_cr2(void)
{
    uint64_t v;
    asm volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr3(void)
{
    uint64_t v;
    asm volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

#endif
