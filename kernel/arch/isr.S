/* one little stub per interrupt vector. they all push the vector number
   (and a fake error code if the cpu didn't push one) and jump to isr_common */

.altmacro

/* vectors where the cpu pushes an error code itself */
.macro make_isr n
isr\n:
.if (\n == 8) || (\n == 10) || (\n == 11) || (\n == 12) || (\n == 13) || (\n == 14) || (\n == 17) || (\n == 21) || (\n == 29) || (\n == 30)
    pushq $\n
.else
    pushq $0
    pushq $\n
.endif
    jmp isr_common
.endm

.macro isr_addr n
    .quad isr\n
.endm

.text
.set i, 0
.rept 256
    make_isr %i
    .set i, i + 1
.endr

isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    cld
    mov %rsp, %rdi              /* struct trap_frame * */
    call interrupt_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    add $16, %rsp               /* skip vector + error code */
    iretq

/* table of all the stub addresses so idt.c can fill in the idt */
.section .rodata
.global isr_table
isr_table:
.set i, 0
.rept 256
    isr_addr %i
    .set i, i + 1
.endr

.section .note.GNU-stack, "", @progbits
