#include "kernel.h"
#include "multiboot2.h"
#include "arch/arch.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

static const char *cmdline = "";

static const char *mmap_type_name(uint32_t type)
{
    switch (type) {
    case 1: return "usable";
    case 3: return "acpi";
    case 4: return "acpi nvs";
    case 5: return "bad";
    default: return "reserved";
    }
}

static void dump_boot_info(struct mb2_info *mbi)
{
    uint64_t usable = 0;

    for (struct mb2_tag *tag = (struct mb2_tag *)(mbi + 1);
         tag->type != MB2_TAG_END;
         tag = MB2_NEXT_TAG(tag)) {

        if (tag->type == MB2_TAG_LOADER) {
            kprintf("bootloader: %s\n", ((struct mb2_tag_string *)tag)->str);
        } else if (tag->type == MB2_TAG_CMDLINE) {
            cmdline = ((struct mb2_tag_string *)tag)->str;
            kprintf("cmdline: '%s'\n", cmdline);
        } else if (tag->type == MB2_TAG_MMAP) {
            struct mb2_tag_mmap *mm = (struct mb2_tag_mmap *)tag;
            kprintf("memory map:\n");
            for (uint8_t *p = (uint8_t *)mm->entries;
                 p < (uint8_t *)mm + mm->size;
                 p += mm->entry_size) {
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
                kprintf("  %016lx - %016lx  %s\n", e->addr, e->addr + e->len - 1,
                        mmap_type_name(e->type));
                if (e->type == MB2_MEM_AVAILABLE)
                    usable += e->len;
            }
        }
    }
    kprintf("usable ram: %lu MB\n", usable / (1024 * 1024));
}

// echo a typed char back. serial terminals send 0x7f for backspace
static void echo(int c)
{
    if (c == '\r')
        c = '\n';
    if (c == 0x7F || c == '\b') {
        kprintf("\b \b");
        return;
    }
    console_putc(c);
}

void kmain(uint32_t magic, uint32_t mbi_phys)
{
    serial_init();
    vga_init();

    kprintf("minos booting...\n");
    kprintf("hello from 64 bit long mode! kmain is at %p\n", (void *)kmain);

    if (magic != MB2_BOOT_MAGIC)
        panic("bad multiboot magic %x", magic);

    dump_boot_info(PHYS_TO_VIRT(mbi_phys));

    gdt_init();
    kprintf("gdt + tss loaded\n");
    idt_init();
    kprintf("idt loaded\n");

    // memory. vmm has to come after gdt_init since it removes the low
    // identity map, and the gdt from boot.S lives down there
    struct mb2_info *mbi = PHYS_TO_VIRT(mbi_phys);
    pmm_init(mbi, mbi_phys);
    vmm_init(mbi);
    pmm_self_test();
    vmm_self_test();
    pic_init();
    pit_init();
    kbd_init();
    sti();
    kprintf("interrupts on\n");

    // boot with "ud2test" on the grub command line to check exceptions work
    if (strcmp(cmdline, "ud2test") == 0) {
        kprintf("running ud2 on purpose...\n");
        asm volatile("ud2");
    }

    // make sure the timer is actually ticking
    uint64_t start = timer_ticks();
    while (timer_ticks() - start < TIMER_HZ)
        hlt();
    kprintf("timer works, %d ticks went by in about 1 second\n", TIMER_HZ);

    kprintf("type something (keyboard or serial):\n> ");
    for (;;) {
        int c;
        while ((c = kbd_getc()) != -1)
            echo(c);
        while ((c = serial_getc()) != -1)
            echo(c);
        hlt();   // sleep until the next interrupt
    }
}
