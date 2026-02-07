#ifndef _MM_HEAP_H
#define _MM_HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init(uint32_t start, uint32_t size);
void *kmalloc(uint32_t size);
void *kmalloc_aligned(uint32_t size, uint32_t alignment);
void kfree(void *ptr);
void kfree_aligned(void *ptr);
int heap_check_overflow(void *ptr);
void heap_get_stats(uint32_t *allocs, uint32_t *frees, uint32_t *current, 
                    uint32_t *bytes, uint32_t *peak_allocs, uint32_t *peak_b);
int heap_check_leaks(void);

#endif
