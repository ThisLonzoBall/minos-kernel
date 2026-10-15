#include "arch/arch.h"

#define PIT_FREQ 1193182

static volatile uint64_t ticks;

static void timer_irq(struct trap_frame *tf)
{
    (void)tf;
    ticks++;
}

void pit_init(void)
{
    uint16_t divisor = PIT_FREQ / TIMER_HZ;
    outb(0x43, 0x36);   // channel 0, lo/hi byte, square wave
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
    irq_register(IRQ_TIMER, timer_irq);
}

uint64_t timer_ticks(void)
{
    return ticks;
}
