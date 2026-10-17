#include "mm/pmm.h"
#include "multiboot2.h"

// physical page allocator. one bit per 4k page, 1 = used, 0 = free.
//
// handles up to 4GB of ram, that's plenty for qemu. the bitmap is 128KB.
// careful: until vmm_init runs only the first 1GB is actually mapped,
// so nothing should touch pages above that before then
#define MAX_PHYS   (4ULL << 30)
#define MAX_PAGES  (MAX_PHYS / PAGE_SIZE)

static uint64_t bitmap[MAX_PAGES / 64];
static uint64_t total_pages;   // pages we actually manage (usable ram)
static uint64_t free_pages;
static uint64_t next_hint;     // where to start looking next time

extern char _kernel_end[];

static inline bool page_used(uint64_t page)
{
    return bitmap[page / 64] & (1ULL << (page % 64));
}

static inline void set_used(uint64_t page)
{
    bitmap[page / 64] |= 1ULL << (page % 64);
}

static inline void set_free(uint64_t page)
{
    bitmap[page / 64] &= ~(1ULL << (page % 64));
}

// mark [start, end) as used, if it's not already
static void reserve_range(uint64_t start, uint64_t end)
{
    start = ALIGN_DOWN(start, PAGE_SIZE);
    end = ALIGN_UP(end, PAGE_SIZE);
    if (end > MAX_PHYS)
        end = MAX_PHYS;

    for (uint64_t a = start; a < end; a += PAGE_SIZE) {
        uint64_t page = a / PAGE_SIZE;
        if (!page_used(page)) {
            set_used(page);
            free_pages--;
            total_pages--;
        }
    }
}

void pmm_init(struct mb2_info *mbi, uint64_t mbi_phys)
{
    // start with everything marked used, then free what grub says is usable
    memset(bitmap, 0xFF, sizeof(bitmap));

    struct mb2_tag_mmap *mm = (struct mb2_tag_mmap *)mb2_find_tag(mbi, MB2_TAG_MMAP);
    if (!mm)
        panic("pmm: grub didn't give us a memory map");

    for (uint8_t *p = (uint8_t *)mm->entries; p < (uint8_t *)mm + mm->size; p += mm->entry_size) {
        struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
        if (e->type != MB2_MEM_AVAILABLE)
            continue;

        // only use whole pages inside the region
        uint64_t start = ALIGN_UP(e->addr, PAGE_SIZE);
        uint64_t end = ALIGN_DOWN(e->addr + e->len, PAGE_SIZE);
        if (end > MAX_PHYS)
            end = MAX_PHYS;

        for (uint64_t a = start; a < end; a += PAGE_SIZE) {
            uint64_t page = a / PAGE_SIZE;
            if (page_used(page)) {
                set_free(page);
                free_pages++;
                total_pages++;
            }
        }
    }

    // now take back the stuff that's already in use:
    // low 1MB (bios junk, vga memory, and page 0 so 0 can mean "no page")
    reserve_range(0, 0x100000);
    // the kernel itself
    reserve_range(0x100000, (uint64_t)_kernel_end - KERNEL_VMA);
    // grub's boot info, we still read it later
    reserve_range(mbi_phys, mbi_phys + mbi->total_size);

    next_hint = 0;
    kprintf("pmm: %lu MB free (%lu pages), kernel ends at %lx\n",
            free_pages * PAGE_SIZE / (1024 * 1024), free_pages,
            (uint64_t)_kernel_end - KERNEL_VMA);
}

uint64_t pmm_alloc(void)
{
    if (free_pages == 0)
        return 0;

    // look for a word that isn't all 1s, starting at the hint
    uint64_t words = MAX_PAGES / 64;
    for (uint64_t n = 0; n < words; n++) {
        uint64_t w = (next_hint + n) % words;
        if (bitmap[w] == ~0ULL)
            continue;

        // find the first 0 bit
        int bit = __builtin_ctzll(~bitmap[w]);
        uint64_t page = w * 64 + bit;
        set_used(page);
        free_pages--;
        next_hint = w;
        return page * PAGE_SIZE;
    }
    return 0;
}

void pmm_free(uint64_t phys)
{
    if (phys % PAGE_SIZE)
        panic("pmm_free: %lx isn't page aligned", phys);
    if (phys >= MAX_PHYS)
        panic("pmm_free: %lx is out of range", phys);

    uint64_t page = phys / PAGE_SIZE;
    if (!page_used(page))
        panic("pmm_free: double free of %lx", phys);

    set_free(page);
    free_pages++;
    if (page / 64 < next_hint)
        next_hint = page / 64;
}

uint64_t pmm_free_count(void)
{
    return free_pages;
}

uint64_t pmm_total_count(void)
{
    return total_pages;
}

// quick sanity check at boot: grab some pages, write to them, give them back
void pmm_self_test(void)
{
    uint64_t pages[64];
    uint64_t before = free_pages;

    for (int i = 0; i < 64; i++) {
        pages[i] = pmm_alloc();
        if (!pages[i])
            panic("pmm test: ran out of memory");
        if (pages[i] < 0x100000)
            panic("pmm test: got a page in low memory %lx", pages[i]);

        // make sure we didn't get the same page twice
        for (int j = 0; j < i; j++) {
            if (pages[j] == pages[i])
                panic("pmm test: page %lx handed out twice", pages[i]);
        }

        // write to it to make sure it's real memory
        uint64_t *p = PHYS_TO_VIRT(pages[i]);
        p[0] = 0xDEADBEEF00000000ULL | i;
        p[511] = i;
    }

    for (int i = 0; i < 64; i++) {
        uint64_t *p = PHYS_TO_VIRT(pages[i]);
        if (p[0] != (0xDEADBEEF00000000ULL | i) || p[511] != (uint64_t)i)
            panic("pmm test: page %lx got corrupted", pages[i]);
    }

    for (int i = 0; i < 64; i++)
        pmm_free(pages[i]);

    if (free_pages != before)
        panic("pmm test: leaked pages (%lu before, %lu after)", before, free_pages);

    kprintf("pmm: self test ok (first page was %lx)\n", pages[0]);
}
