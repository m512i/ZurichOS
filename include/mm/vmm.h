#ifndef _MM_VMM_H
#define _MM_VMM_H

#include <stdint.h>

#define PAGE_PRESENT    0x001
#define PAGE_WRITE      0x002
#define PAGE_USER       0x004
#define PAGE_PWT        0x008   
#define PAGE_PCD        0x010   
#define PAGE_ACCESSED   0x020
#define PAGE_DIRTY      0x040
#define PAGE_SIZE_4M    0x080
#define PAGE_GLOBAL     0x100

#define PAGE_KERNEL     (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_USERSPACE  (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

void vmm_init(void);
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void vmm_unmap_page(uint32_t virt);
uint32_t vmm_get_physical(uint32_t virt);
int vmm_is_mapped(uint32_t virt);
void vmm_flush_tlb(void);
void vmm_invlpg(uint32_t virt);

uint32_t *vmm_get_current_pagedir(void);

#endif