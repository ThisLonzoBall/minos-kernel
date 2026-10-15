#include "arch/arch.h"

// the two old 8259 PICs. master handles irq 0-7, slave 8-15
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static irq_handler_t handlers[16];

// tiny delay, some old hardware needs it between pic writes
static void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    // by default the pic uses vectors 0-15 which clash with cpu exceptions,
    // so move them to 32-47
    outb(PIC1_CMD, 0x11);  io_wait();
    outb(PIC2_CMD, 0x11);  io_wait();
    outb(PIC1_DATA, IRQ_BASE);      io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);  io_wait();
    outb(PIC1_DATA, 4);    io_wait();   // slave is on irq 2
    outb(PIC2_DATA, 2);    io_wait();
    outb(PIC1_DATA, 0x01); io_wait();   // 8086 mode
    outb(PIC2_DATA, 0x01); io_wait();

    // mask everything, drivers unmask what they need
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    pic_unmask(2);   // cascade line so the slave can talk
}

void pic_unmask(int irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8)
        irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}

void pic_eoi(int irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void irq_register(int irq, irq_handler_t fn)
{
    handlers[irq] = fn;
    pic_unmask(irq);
}

void irq_dispatch(int irq, struct trap_frame *tf)
{
    // irq 7 and 15 can be fake ("spurious"). check the in-service register
    if (irq == 7 || irq == 15) {
        uint16_t cmd = irq == 7 ? PIC1_CMD : PIC2_CMD;
        outb(cmd, 0x0B);
        if (!(inb(cmd) & 0x80)) {
            if (irq == 15)
                outb(PIC1_CMD, 0x20);   // master still needs its eoi
            return;
        }
    }

    // send eoi first. interrupts stay off until iretq anyway, and later the
    // timer handler might switch to another process and not come back here for a while
    pic_eoi(irq);

    if (handlers[irq])
        handlers[irq](tf);
}
