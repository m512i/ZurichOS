/* Memory Mapping Header
 * mmap/munmap/mprotect definitions
 */

#ifndef _MM_MMAP_H
#define _MM_MMAP_H

#include <stdint.h>

#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

#define MAX_VMAS_PER_PROC   32

typedef struct vma {
    uint32_t start;
    uint32_t end;           
    uint32_t prot;          
    uint32_t flags;         
    uint32_t file_offset;   
    void *file;             
    uint8_t in_use;         
    uint8_t cow;            
    uint8_t lazy;           
} vma_t;

void *sys_mmap(void *addr, uint32_t length, int prot, int flags, int fd, uint32_t offset);
int sys_munmap(void *addr, uint32_t length);
int sys_mprotect(void *addr, uint32_t length, int prot);

void vma_init_process(void *proc);
vma_t *vma_find(uint32_t addr);
vma_t *vma_create(uint32_t start, uint32_t end, uint32_t prot, uint32_t flags);
int vma_destroy(vma_t *vma);

int handle_page_fault(uint32_t fault_addr, uint32_t error_code);

int cow_handle_fault(uint32_t fault_addr);
int cow_mark_page(uint32_t virt);

int demand_page_alloc(uint32_t virt, vma_t *vma);

int stack_grow(uint32_t fault_addr);

#endif
