#define KERNEL_VMA 0xFFFFFFFF80000000
#define V2P(x) ((x) - KERNEL_VMA)

/* grub looks for this header to know we're multiboot2 */
.section .multiboot, "a"
.align 8
mb_header:
    .long 0xE85250D6                    /* magic */
    .long 0                             /* i386 */
    .long mb_header_end - mb_header
    .long -(0xE85250D6 + (mb_header_end - mb_header))
    /* end tag */
    .align 8
    .short 0
    .short 0
    .long 8
mb_header_end:

.section .boot.data, "aw"
.align 16
gdt64:
    .quad 0                             /* null */
    .quad 0x00AF9A000000FFFF            /* 0x08 kernel code (64 bit) */
    .quad 0x00CF92000000FFFF            /* 0x10 kernel data */
gdt64_end:
gdt64_ptr:
    .short gdt64_end - gdt64 - 1
    .quad gdt64

saved_magic: .long 0
saved_mbi:   .long 0

/* grub drops us here in 32 bit protected mode, paging off */
.section .boot.text, "ax"
.code32
.global _start
_start:
    cli
    mov $V2P(boot_stack_top), %esp

    /* stash what grub gave us, we need it later in kmain */
    mov %eax, saved_magic
    mov %ebx, saved_mbi

    cmp $0x36D76289, %eax
    jne no_multiboot

    /* clear the page tables (4 pages) just in case */
    mov $V2P(boot_pml4), %edi
    xor %eax, %eax
    mov $(4096 * 4 / 4), %ecx
    rep stosl

    /* pml4[0] -> low pdpt (identity map), pml4[511] -> high pdpt */
    mov $V2P(boot_pdpt_low), %eax
    or $3, %eax
    mov %eax, V2P(boot_pml4)
    mov $V2P(boot_pdpt_high), %eax
    or $3, %eax
    mov %eax, V2P(boot_pml4) + 511 * 8

    /* both pdpts point at the same page directory.
       0xFFFFFFFF80000000 is pdpt index 510 */
    mov $V2P(boot_pd), %eax
    or $3, %eax
    mov %eax, V2P(boot_pdpt_low)
    mov %eax, V2P(boot_pdpt_high) + 510 * 8

    /* fill the page directory with 512 x 2MB pages = first 1GB */
    mov $V2P(boot_pd), %edi
    mov $0x83, %eax                     /* present | writable | huge */
    mov $512, %ecx
1:
    mov %eax, (%edi)
    movl $0, 4(%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 1b

    /* load cr3 */
    mov $V2P(boot_pml4), %eax
    mov %eax, %cr3

    /* turn on PAE */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* set long mode enable in EFER msr */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* paging on (and protected mode, already on) */
    mov %cr0, %eax
    or $((1 << 31) | 1), %eax
    mov %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $long_mode_start

no_multiboot:
    /* print "ERR" on the screen and give up */
    movl $0x4F524F45, 0xB8000
    movw $0x4F52, 0xB8004
1:  hlt
    jmp 1b

.code64
long_mode_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %ax, %ax
    mov %ax, %fs
    mov %ax, %gs

    /* movl zero extends into the full 64 bit reg */
    movl saved_magic, %edi
    movl saved_mbi, %esi

    /* jump up to the higher half */
    movabs $higher_half, %rax
    jmp *%rax

.text
higher_half:
    movabs $boot_stack_top, %rsp
    xor %rbp, %rbp
    call kmain
1:  cli
    hlt
    jmp 1b

.bss
.align 4096
.global boot_pml4
boot_pml4:      .skip 4096
boot_pdpt_low:  .skip 4096
boot_pdpt_high: .skip 4096
boot_pd:        .skip 4096

.align 16
boot_stack:     .skip 16384
.global boot_stack_top
boot_stack_top:

.section .note.GNU-stack, "", @progbits
