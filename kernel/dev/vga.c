#include "kernel.h"

#define VGA_W 80
#define VGA_H 25

static uint16_t *vga_buf;
static int row, col;
static uint8_t color = 0x07;   // light grey on black

static void update_cursor(void)
{
    uint16_t pos = row * VGA_W + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
}

static void scroll(void)
{
    memmove(vga_buf, vga_buf + VGA_W, (VGA_H - 1) * VGA_W * 2);
    for (int i = 0; i < VGA_W; i++)
        vga_buf[(VGA_H - 1) * VGA_W + i] = (color << 8) | ' ';
    row = VGA_H - 1;
}

void vga_init(void)
{
    vga_buf = PHYS_TO_VIRT(0xB8000);
    for (int i = 0; i < VGA_W * VGA_H; i++)
        vga_buf[i] = (color << 8) | ' ';
    row = col = 0;
    update_cursor();
}

void vga_putc(char c)
{
    if (!vga_buf)
        return;

    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            vga_buf[row * VGA_W + col] = (color << 8) | ' ';
        }
    } else if (c == '\t') {
        col = (col + 8) & ~7;
    } else {
        vga_buf[row * VGA_W + col] = (color << 8) | (uint8_t)c;
        col++;
    }

    if (col >= VGA_W) {
        col = 0;
        row++;
    }
    if (row >= VGA_H)
        scroll();
    update_cursor();
}

void console_putc(char c)
{
    serial_putc(c);
    vga_putc(c);
}
