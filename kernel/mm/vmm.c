/* Virtual Memory Manager (VMM)
 * Manages page tables and virtual address mappings
 */

#include <kernel/kernel.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

static uint32_t *current_page_dir = NULL;

#define RECURSIVE_PD_INDEX      1023
#define RECURSIVE_PD_ADDR       0xFFFFF000
#define RECURSIVE_PT_BASE       0xFFC00000

#define GET_PT(pd_index)        ((uint32_t *)(RECURSIVE_PT_BASE + ((pd_index) * PAGE_SIZE)))
#define GET_PD()                ((uint32_t *)RECURSIVE_PD_ADDR)

extern uint32_t boot_page_directory;

void vmm_init(void)
{
    current_page_dir = &boot_page_directory;
    
    uint32_t *pd = (uint32_t *)((uint32_t)current_page_dir + KERNEL_VMA);
    pd[RECURSIVE_PD_INDEX] = (uint32_t)current_page_dir | PAGE_PRESENT | PAGE_WRITE;
    
    vmm_flush_tlb();
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pd_index = (virt >> 22) & 0x3FF;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    
    uint32_t *pd = GET_PD();
    
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) {
            return;
        }
        
        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
        
        vmm_invlpg((uint32_t)GET_PT(pd_index));
        
        uint32_t *pt = GET_PT(pd_index);
        for (int i = 0; i < 1024; i++) {
            pt[i] = 0;
        }
    } else if (flags & PAGE_USER) {
        pd[pd_index] |= PAGE_USER;
    }
    
    uint32_t *pt = GET_PT(pd_index);
    pt[pt_index] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    
    vmm_invlpg(virt);
}

void vmm_unmap_page(uint32_t virt)
{
    uint32_t pd_index = (virt >> 22) & 0x3FF;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    
    uint32_t *pd = GET_PD();
    
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        return;
    }
    
    uint32_t *pt = GET_PT(pd_index);
    pt[pt_index] = 0;
    
    vmm_invlpg(virt);
}

uint32_t vmm_get_physical(uint32_t virt)
{
    uint32_t pd_index = (virt >> 22) & 0x3FF;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t *pd = GET_PD();
    
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    uint32_t *pt = GET_PT(pd_index);
    
    if (!(pt[pt_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    return (pt[pt_index] & 0xFFFFF000) | (virt & 0xFFF);
}

int vmm_is_mapped(uint32_t virt)
{
    uint32_t pd_index = (virt >> 22) & 0x3FF;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    
    uint32_t *pd = GET_PD();
    
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    uint32_t *pt = GET_PT(pd_index);
    return (pt[pt_index] & PAGE_PRESENT) != 0;
}

void vmm_flush_tlb(void)
{
    __asm__ volatile(
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        : : : "eax"
    );
}

void vmm_invlpg(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint32_t *vmm_get_current_pagedir(void)
{
    return GET_PD();
}
