#ifndef PMM_H
#define PMM_H

#include "kernel.h"

#define PAGE_SIZE 4096

struct mb2_info;

void pmm_init(struct mb2_info *mbi, uint64_t mbi_phys);
uint64_t pmm_alloc(void);          // returns a physical address, 0 if we're out
void pmm_free(uint64_t phys);
uint64_t pmm_free_count(void);
uint64_t pmm_total_count(void);
void pmm_self_test(void);

#endif
