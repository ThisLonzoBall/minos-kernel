#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define KERNEL_VMA 0xFFFFFFFF80000000ULL

// all of ram gets mapped here once vmm_init runs
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

// at boot only the first 1GB is mapped (at KERNEL_VMA). vmm_init switches
// this over to DIRECT_MAP_BASE so we can reach all of ram
extern uint64_t phys_map_base;
#define PHYS_TO_VIRT(p) ((void *)((uint64_t)(p) + phys_map_base))
#define VIRT_TO_PHYS(v) ((uint64_t)(v) - phys_map_base)

// for addresses inside the kernel image itself (code, globals)
#define KERNEL_VIRT_TO_PHYS(v) ((uint64_t)(v) - KERNEL_VMA)

#define ALIGN_UP(x, a)   (((x) + (a) - 1) & ~((uint64_t)(a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((uint64_t)(a) - 1))

// string.c
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strncpy(char *dst, const char *src, size_t n);

// printf.c
int kprintf(const char *fmt, ...);
int vkprintf(const char *fmt, va_list ap);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
void panic(const char *fmt, ...) __attribute__((noreturn));

// console stuff (serial + vga)
void serial_init(void);
void serial_putc(char c);
int serial_getc(void);   // -1 if nothing there
void vga_init(void);
void vga_putc(char c);
void console_putc(char c);

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void hlt(void) { asm volatile("hlt"); }
static inline void cli(void) { asm volatile("cli"); }
static inline void sti(void) { asm volatile("sti"); }

#endif
