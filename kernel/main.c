#include "kernel.h"
#include "multiboot2.h"

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
            kprintf("cmdline: '%s'\n", ((struct mb2_tag_string *)tag)->str);
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

void kmain(uint32_t magic, uint32_t mbi_phys)
{
    serial_init();
    vga_init();

    kprintf("minos booting...\n");
    kprintf("hello from 64 bit long mode! kmain is at %p\n", (void *)kmain);

    if (magic != MB2_BOOT_MAGIC)
        panic("bad multiboot magic %x", magic);

    dump_boot_info(PHYS_TO_VIRT(mbi_phys));

    kprintf("nothing else to do yet, halting\n");
    for (;;)
        hlt();
}
