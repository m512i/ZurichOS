/* Memory Mapping
 * mmap, munmap, mprotect, COW, demand paging
 */

#include <mm/mmap.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/process.h>
#include <drivers/serial.h>
#include <string.h>

/* Per-process VMA table (simplified - in real OS this would be in process_t) */
static vma_t process_vmas[64][MAX_VMAS_PER_PROC];

#define USER_MMAP_START     0x40000000
#define USER_MMAP_END       0x80000000
#define USER_STACK_TOP      0xBFFFF000
#define USER_STACK_BOTTOM   0xBF800000  

static uint32_t next_mmap_addr = USER_MMAP_START;

void vma_init_process(void *p)
{
    process_t *proc = (process_t *)p;
    if (!proc || proc->pid >= 64) return;
    
    memset(process_vmas[proc->pid], 0, sizeof(vma_t) * MAX_VMAS_PER_PROC);
}

static vma_t *get_vma_table(void)
{
    process_t *proc = process_current();
    if (!proc || proc->pid >= 64) return NULL;
    return process_vmas[proc->pid];
}

vma_t *vma_find(uint32_t addr)
{
    vma_t *vmas = get_vma_table();
    if (!vmas) return NULL;
    
    for (int i = 0; i < MAX_VMAS_PER_PROC; i++) {
        if (vmas[i].in_use && addr >= vmas[i].start && addr < vmas[i].end) {
            return &vmas[i];
        }
    }
    return NULL;
}

vma_t *vma_create(uint32_t start, uint32_t end, uint32_t prot, uint32_t flags)
{
    vma_t *vmas = get_vma_table();
    if (!vmas) return NULL;
    
    for (int i = 0; i < MAX_VMAS_PER_PROC; i++) {
        if (!vmas[i].in_use) {
            vmas[i].start = start;
            vmas[i].end = end;
            vmas[i].prot = prot;
            vmas[i].flags = flags;
            vmas[i].file = NULL;
            vmas[i].file_offset = 0;
            vmas[i].in_use = 1;
            vmas[i].cow = 0;
            vmas[i].lazy = (flags & MAP_ANONYMOUS) ? 1 : 0;
            return &vmas[i];
        }
    }
    return NULL;
}

int vma_destroy(vma_t *vma)
{
    if (!vma) return -1;
    vma->in_use = 0;
    return 0;
}

void *sys_mmap(void *addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)
{
    (void)fd;
    (void)offset;
    
    if (length == 0) return MAP_FAILED;
    
    length = (length + 0xFFF) & ~0xFFF;
    
    uint32_t vaddr;
    
    if (flags & MAP_FIXED) {
        vaddr = (uint32_t)addr;
        if (vaddr < USER_MMAP_START || vaddr + length > USER_MMAP_END) {
            return MAP_FAILED;
        }
    } else {
        vaddr = next_mmap_addr;
        if (vaddr + length > USER_MMAP_END) {
            return MAP_FAILED;
        }
        next_mmap_addr += length;
    }
    
    vma_t *vma = vma_create(vaddr, vaddr + length, prot, flags);
    if (!vma) {
        return MAP_FAILED;
    }
    
    uint32_t page_flags = PAGE_USER | PAGE_PRESENT | PAGE_WRITE;
    (void)prot;  
    
    if (flags & MAP_ANONYMOUS) {
        /* Anonymous mapping - always allocate immediately for now */
        /* (Lazy allocation requires page fault handler integration) */
        {
            for (uint32_t page = vaddr; page < vaddr + length; page += 0x1000) {
                uint32_t phys = pmm_alloc_frame();
                if (!phys) {
                    for (uint32_t p = vaddr; p < page; p += 0x1000) {
                        uint32_t ph = vmm_get_physical(p);
                        vmm_unmap_page(p);
                        pmm_free_frame(ph);
                    }
                    vma_destroy(vma);
                    return MAP_FAILED;
                }
                serial_puts("[MMAP] Mapping page 0x");
                char hex[9];
                for (int k = 7; k >= 0; k--) {
                    int nibble = (page >> (k * 4)) & 0xF;
                    hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
                }
                hex[8] = '\0';
                serial_puts(hex);
                serial_puts(" -> phys 0x");
                for (int k = 7; k >= 0; k--) {
                    int nibble = (phys >> (k * 4)) & 0xF;
                    hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
                }
                serial_puts(hex);
                serial_puts("\n");
                
                vmm_map_page(page, phys, page_flags);
                
                if (!vmm_is_mapped(page)) {
                    serial_puts("[MMAP] ERROR: Page not mapped!\n");
                }
                
                memset((void *)page, 0, 0x1000);
            }
            serial_puts("[MMAP] Created anonymous mapping\n");
        }
    } else {
        serial_puts("[MMAP] File mapping not fully implemented\n");
        vma_destroy(vma);
        return MAP_FAILED;
    }
    
    return (void *)vaddr;
}

int sys_munmap(void *addr, uint32_t length)
{
    uint32_t vaddr = (uint32_t)addr;
    
    if (vaddr & 0xFFF) return -22;  
    if (length == 0) return -22;
    
    length = (length + 0xFFF) & ~0xFFF;
    
    vma_t *vma = vma_find(vaddr);
    if (!vma) return -22;
    
    for (uint32_t page = vaddr; page < vaddr + length; page += 0x1000) {
        if (vmm_is_mapped(page)) {
            uint32_t phys = vmm_get_physical(page);
            vmm_unmap_page(page);
            
            if (!(vma->flags & MAP_SHARED)) {
                pmm_free_frame(phys);
            }
        }
    }
    
    if (vaddr == vma->start && vaddr + length == vma->end) {
        vma_destroy(vma);
    } else {
        if (vaddr == vma->start) {
            vma->start = vaddr + length;
        } else if (vaddr + length == vma->end) {
            vma->end = vaddr;
        }
    }
    
    serial_puts("[MMAP] Unmapped region\n");
    return 0;
}

int sys_mprotect(void *addr, uint32_t length, int prot)
{
    uint32_t vaddr = (uint32_t)addr;
    
    if (vaddr & 0xFFF) return -22;
    if (length == 0) return -22;
    
    length = (length + 0xFFF) & ~0xFFF;
    
    vma_t *vma = vma_find(vaddr);
    if (!vma) return -22;
    
    vma->prot = prot;
    
    uint32_t page_flags = PAGE_USER | PAGE_PRESENT;
    if (prot & PROT_WRITE) {
        page_flags |= PAGE_WRITE;
    }
    
    for (uint32_t page = vaddr; page < vaddr + length; page += 0x1000) {
        if (vmm_is_mapped(page)) {
            uint32_t phys = vmm_get_physical(page);
            vmm_unmap_page(page);
            vmm_map_page(page, phys, page_flags);
        }
    }
    
    serial_puts("[MMAP] Changed protection\n");
    return 0;
}


int handle_page_fault(uint32_t fault_addr, uint32_t error_code)
{
    serial_puts("[PAGEFAULT] Fault at 0x");
    char hex[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (fault_addr >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[8] = '\0';
    serial_puts(hex);
    serial_puts("\n");
    
    if ((error_code & 0x1) && (error_code & 0x2)) {
        if (cow_handle_fault(fault_addr) == 0) {
            return 0;
        }
    }
    
    vma_t *vma = vma_find(fault_addr);
    if (vma && vma->lazy) {
        if (demand_page_alloc(fault_addr, vma) == 0) {
            return 0;
        }
    }
    
    if (fault_addr >= USER_STACK_BOTTOM && fault_addr < USER_STACK_TOP) {
        if (stack_grow(fault_addr) == 0) {
            return 0;
        }
    }
    
    return -1;
}


int cow_mark_page(uint32_t virt)
{
    if (!vmm_is_mapped(virt)) return -1;
    
    uint32_t phys = vmm_get_physical(virt);
    vmm_unmap_page(virt);
    vmm_map_page(virt, phys, PAGE_USER | PAGE_PRESENT);  
    
    return 0;
}

int cow_handle_fault(uint32_t fault_addr)
{
    uint32_t page_addr = fault_addr & ~0xFFF;
    
    vma_t *vma = vma_find(fault_addr);
    if (!vma || !vma->cow) return -1;
    if (!(vma->prot & PROT_WRITE)) return -1;
    
    uint32_t old_phys = vmm_get_physical(page_addr);
    if (!old_phys) return -1;
    
    uint32_t new_phys = pmm_alloc_frame();
    if (!new_phys) return -12;  
    
    vmm_map_page(0xFFFFE000, new_phys, PAGE_KERNEL);
    memcpy((void *)0xFFFFE000, (void *)page_addr, 0x1000);
    vmm_unmap_page(0xFFFFE000);
    
    vmm_unmap_page(page_addr);
    vmm_map_page(page_addr, new_phys, PAGE_USER | PAGE_PRESENT | PAGE_WRITE);
    
    serial_puts("[COW] Copied page\n");
    return 0;
}


int demand_page_alloc(uint32_t virt, vma_t *vma)
{
    uint32_t page_addr = virt & ~0xFFF;
    
    if (vmm_is_mapped(page_addr)) return -1; 
    
    uint32_t phys = pmm_alloc_frame();
    if (!phys) return -12; 
    
    uint32_t flags = PAGE_USER | PAGE_PRESENT;
    if (vma->prot & PROT_WRITE) {
        flags |= PAGE_WRITE;
    }
    
    vmm_map_page(page_addr, phys, flags);
    
    if (vma->flags & MAP_ANONYMOUS) {
        memset((void *)page_addr, 0, 0x1000);
    }
    
    serial_puts("[DEMAND] Allocated page on fault\n");
    return 0;
}


int stack_grow(uint32_t fault_addr)
{
    uint32_t page_addr = fault_addr & ~0xFFF;
    
    if (vmm_is_mapped(page_addr)) return -1;
    
    uint32_t phys = pmm_alloc_frame();
    if (!phys) return -12;
    
    vmm_map_page(page_addr, phys, PAGE_USER | PAGE_PRESENT | PAGE_WRITE);
    memset((void *)page_addr, 0, 0x1000);
    
    serial_puts("[STACK] Grew stack\n");
    return 0;
}


void mmap_init(void)
{
    memset(process_vmas, 0, sizeof(process_vmas));
    next_mmap_addr = USER_MMAP_START;
    serial_puts("[MMAP] Initialized\n");
}
