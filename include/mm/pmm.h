#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <stdint.h>
#include <kernel/multiboot.h>

void pmm_init(multiboot_info_t *mboot);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t addr);
void pmm_mark_used(uint32_t addr);
uint32_t pmm_get_total_memory(void);
uint32_t pmm_get_used_memory(void);
uint32_t pmm_get_free_memory(void);

#endif
