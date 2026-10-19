#include "mm/kheap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

// kernel heap. just a bump allocator for now, kfree doesn't do anything yet.
// pages get mapped in as the heap grows
#define HEAP_START 0xFFFFD00000000000ULL

static uint64_t heap_top = HEAP_START;      // next free byte
static uint64_t heap_mapped = HEAP_START;   // everything below this is mapped

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = ALIGN_UP(size, 16);

    // map more pages if we need them
    while (heap_top + size > heap_mapped) {
        uint64_t phys = pmm_alloc();
        if (!phys)
            return NULL;
        if (!vmm_map(vmm_kernel_pml4(), heap_mapped, phys, PTE_WRITE)) {
            pmm_free(phys);
            return NULL;
        }
        heap_mapped += PAGE_SIZE;
    }

    void *p = (void *)heap_top;
    heap_top += size;
    return p;
}

void kfree(void *ptr)
{
    // TODO: actually free stuff
    (void)ptr;
}
