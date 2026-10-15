#include "kernel.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);   // no interrupts for now
    outb(COM1 + 3, 0x80);   // DLAB on so we can set the baud rate
    outb(COM1 + 0, 0x01);   // divisor 1 = 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   // 8 bits, no parity, 1 stop bit
    outb(COM1 + 2, 0xC7);   // turn on the fifo
    outb(COM1 + 4, 0x0B);
}

static void serial_raw_putc(char c)
{
    // wait until it's ready to send
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, c);
}

void serial_putc(char c)
{
    if (c == '\n')
        serial_raw_putc('\r');
    serial_raw_putc(c);
}

int serial_getc(void)
{
    if (!(inb(COM1 + 5) & 1))
        return -1;
    return inb(COM1);
}
