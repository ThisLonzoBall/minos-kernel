#ifndef KHEAP_H
#define KHEAP_H

#include "kernel.h"

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
