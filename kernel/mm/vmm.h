#ifndef VMM_H
#define VMM_H

#include "kernel.h"

// page table entry bits
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE   (1ULL << 1)
#define PTE_USER    (1ULL << 2)
#define PTE_HUGE    (1ULL << 7)
#define PTE_ADDR    0x000FFFFFFFFFF000ULL

struct mb2_info;

void vmm_init(struct mb2_info *mbi);
uint64_t *vmm_kernel_pml4(void);

bool vmm_map(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t vmm_unmap(uint64_t *pml4, uint64_t virt);   // returns the phys addr that was there, or 0
bool vmm_translate(uint64_t *pml4, uint64_t virt, uint64_t *phys_out);

void vmm_self_test(void);

#endif
