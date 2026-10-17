#include "mm/vmm.h"
#include "mm/pmm.h"
#include "arch/arch.h"
#include "multiboot2.h"

// starts out pointing at the boot mapping, vmm_init moves it to the direct map
uint64_t phys_map_base = KERNEL_VMA;

// we just keep using the page tables from boot.S and add to them
extern uint64_t boot_pml4[512];
static uint64_t *kernel_pml4 = boot_pml4;

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)

#define SIZE_2M 0x200000ULL

static uint64_t alloc_table(void)
{
    uint64_t phys = pmm_alloc();
    if (!phys)
        return 0;
    // before the direct map is up we can only reach the first 1GB
    if (phys_map_base == KERNEL_VMA && phys >= (1ULL << 30))
        panic("vmm: got page %lx we can't reach yet", phys);
    memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
    return phys;
}

// follow one level down. if it's not there and create is set, make a new table.
// returns NULL if it's missing (or it's a huge page, we can't go into those)
static uint64_t *next_level(uint64_t *table, int idx, bool create)
{
    uint64_t e = table[idx];
    if (e & PTE_PRESENT) {
        if (e & PTE_HUGE)
            return NULL;
        return PHYS_TO_VIRT(e & PTE_ADDR);
    }
    if (!create)
        return NULL;

    uint64_t phys = alloc_table();
    if (!phys)
        return NULL;
    // user bit on the middle levels is fine, the last level decides if user can touch it
    table[idx] = phys | PTE_PRESENT | PTE_WRITE | PTE_USER;
    return PHYS_TO_VIRT(phys);
}

// get a pointer to the last level entry for virt
static uint64_t *walk(uint64_t *pml4, uint64_t virt, bool create)
{
    uint64_t *pdpt = next_level(pml4, PML4_IDX(virt), create);
    if (!pdpt)
        return NULL;
    uint64_t *pd = next_level(pdpt, PDPT_IDX(virt), create);
    if (!pd)
        return NULL;
    uint64_t *pt = next_level(pd, PD_IDX(virt), create);
    if (!pt)
        return NULL;
    return &pt[PT_IDX(virt)];
}

bool vmm_map(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t *pte = walk(pml4, virt, true);
    if (!pte)
        return false;
    if (*pte & PTE_PRESENT)
        panic("vmm_map: %lx is already mapped", virt);

    *pte = (phys & PTE_ADDR) | flags | PTE_PRESENT;
    invlpg(virt);
    return true;
}

uint64_t vmm_unmap(uint64_t *pml4, uint64_t virt)
{
    uint64_t *pte = walk(pml4, virt, false);
    if (!pte || !(*pte & PTE_PRESENT))
        return 0;

    uint64_t phys = *pte & PTE_ADDR;
    *pte = 0;
    invlpg(virt);
    return phys;
}

bool vmm_translate(uint64_t *pml4, uint64_t virt, uint64_t *phys_out)
{
    uint64_t e = pml4[PML4_IDX(virt)];
    if (!(e & PTE_PRESENT))
        return false;

    uint64_t *pdpt = PHYS_TO_VIRT(e & PTE_ADDR);
    e = pdpt[PDPT_IDX(virt)];
    if (!(e & PTE_PRESENT))
        return false;

    uint64_t *pd = PHYS_TO_VIRT(e & PTE_ADDR);
    e = pd[PD_IDX(virt)];
    if (!(e & PTE_PRESENT))
        return false;
    if (e & PTE_HUGE) {
        // 2MB page, the low 21 bits are the offset
        *phys_out = (e & PTE_ADDR & ~(SIZE_2M - 1)) + (virt & (SIZE_2M - 1));
        return true;
    }

    uint64_t *pt = PHYS_TO_VIRT(e & PTE_ADDR);
    e = pt[PT_IDX(virt)];
    if (!(e & PTE_PRESENT))
        return false;
    *phys_out = (e & PTE_ADDR) + (virt & 0xFFF);
    return true;
}

// map one 2MB chunk of the direct map
static void map_2m(uint64_t virt, uint64_t phys)
{
    uint64_t *pdpt = next_level(kernel_pml4, PML4_IDX(virt), true);
    uint64_t *pd = pdpt ? next_level(pdpt, PDPT_IDX(virt), true) : NULL;
    if (!pd)
        panic("vmm: out of memory building the direct map");
    pd[PD_IDX(virt)] = phys | PTE_PRESENT | PTE_WRITE | PTE_HUGE;
}

void vmm_init(struct mb2_info *mbi)
{
    // figure out how far up ram goes (usable + acpi stuff, skip the weird reserved holes)
    struct mb2_tag_mmap *mm = (struct mb2_tag_mmap *)mb2_find_tag(mbi, MB2_TAG_MMAP);
    uint64_t top = 0;
    for (uint8_t *p = (uint8_t *)mm->entries; p < (uint8_t *)mm + mm->size; p += mm->entry_size) {
        struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
        if (e->type == 1 || e->type == 3 || e->type == 4) {
            if (e->addr + e->len > top)
                top = e->addr + e->len;
        }
    }
    top = ALIGN_UP(top, SIZE_2M);

    // the tables we make here come from pmm, which hands out low pages first,
    // so they're still reachable through the old 1GB boot mapping
    for (uint64_t phys = 0; phys < top; phys += SIZE_2M)
        map_2m(DIRECT_MAP_BASE + phys, phys);

    phys_map_base = DIRECT_MAP_BASE;

    // don't need the identity map at 0 anymore. after this a null pointer
    // actually faults instead of reading low memory
    kernel_pml4[0] = 0;
    write_cr3(KERNEL_VIRT_TO_PHYS(kernel_pml4));

    kprintf("vmm: mapped %lu MB of ram at %lx, identity map removed\n",
            top / (1024 * 1024), DIRECT_MAP_BASE);
}

uint64_t *vmm_kernel_pml4(void)
{
    return kernel_pml4;
}

void vmm_self_test(void)
{
    uint64_t phys, got;

    // map a fresh page somewhere nobody uses and poke at it
    uint64_t virt = 0xFFFFC00000000000ULL;
    phys = pmm_alloc();
    if (!vmm_map(kernel_pml4, virt, phys, PTE_WRITE))
        panic("vmm test: map failed");

    if (!vmm_translate(kernel_pml4, virt + 0x123, &got) || got != phys + 0x123)
        panic("vmm test: translate gave %lx, wanted %lx", got, phys + 0x123);

    // write through the new mapping, read it back through the direct map
    *(volatile uint64_t *)virt = 0x1234567887654321ULL;
    if (*(uint64_t *)PHYS_TO_VIRT(phys) != 0x1234567887654321ULL)
        panic("vmm test: the two mappings don't see the same memory");

    if (vmm_unmap(kernel_pml4, virt) != phys)
        panic("vmm test: unmap returned the wrong page");
    if (vmm_translate(kernel_pml4, virt, &got))
        panic("vmm test: still mapped after unmap");
    pmm_free(phys);

    // the kernel and direct map should translate to what we expect
    if (!vmm_translate(kernel_pml4, (uint64_t)vmm_self_test, &got) ||
        got != (uint64_t)vmm_self_test - KERNEL_VMA)
        panic("vmm test: kernel code doesn't translate right");
    if (!vmm_translate(kernel_pml4, DIRECT_MAP_BASE + 0x1234000, &got) || got != 0x1234000)
        panic("vmm test: direct map doesn't translate right");

    // and low memory should be gone
    if (vmm_translate(kernel_pml4, 0x100000, &got))
        panic("vmm test: identity map is still there");

    kprintf("vmm: self test ok\n");
}
