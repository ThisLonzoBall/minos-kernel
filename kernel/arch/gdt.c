#include "arch/arch.h"

// 64 bit tss. we only really care about rsp0 (stack to use when coming
// from user mode) and ist1 (separate stack for double faults)
struct tss {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// null, kernel code, kernel data, user data, user code, tss (takes 2 slots)
// user data comes before user code because syscall/sysret wants it that way
static uint64_t gdt[7] = {
    0,
    0x00AF9A000000FFFF,   // 0x08 kernel code
    0x00CF92000000FFFF,   // 0x10 kernel data
    0x00CFF2000000FFFF,   // 0x18 user data (dpl 3)
    0x00AFFA000000FFFF,   // 0x20 user code (dpl 3)
    0, 0                  // 0x28 tss, filled in below
};

static struct tss tss;
static uint8_t double_fault_stack[4096] __attribute__((aligned(16)));

void gdt_init(void)
{
    tss.iomap_base = sizeof(tss);
    tss.ist[0] = (uint64_t)double_fault_stack + sizeof(double_fault_stack);

    // the tss descriptor is 16 bytes and the base address is split all over the place
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    gdt[5] = (limit & 0xFFFF)
           | ((base & 0xFFFFFF) << 16)
           | (0x89ULL << 40)                 // present, 64 bit tss
           | (((limit >> 16) & 0xF) << 48)
           | (((base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;

    struct gdt_ptr ptr = { sizeof(gdt) - 1, (uint64_t)gdt };

    // load the new gdt, then reload cs with a far return and the other segment regs
    asm volatile(
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "xor %%ax, %%ax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : "m"(ptr) : "rax", "memory");

    asm volatile("ltr %w0" : : "r"(TSS_SEL));
}

void tss_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
