#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

#define MB2_BOOT_MAGIC 0x36D76289

#define MB2_TAG_END     0
#define MB2_TAG_CMDLINE 1
#define MB2_TAG_LOADER  2
#define MB2_TAG_MODULE  3
#define MB2_TAG_MEMINFO 4
#define MB2_TAG_MMAP    6

#define MB2_MEM_AVAILABLE 1

struct mb2_info {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed));

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_tag_string {
    uint32_t type;
    uint32_t size;
    char str[];
} __attribute__((packed));

struct mb2_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[];
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct mb2_mmap_entry entries[];
} __attribute__((packed));

// tags are padded to 8 bytes
#define MB2_NEXT_TAG(t) ((struct mb2_tag *)((uint8_t *)(t) + (((t)->size + 7) & ~7)))

#endif
