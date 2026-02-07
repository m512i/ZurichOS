/* Kernel Heap Allocator
 * Simple first-fit allocator for kernel memory
 */

#include <kernel/kernel.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

typedef struct heap_block {
    uint32_t size;              
    uint32_t user_size;         
    uint32_t magic;            
    struct heap_block *next;    
    struct heap_block *prev;    
    uint8_t free;               
} heap_block_t;

#define HEAP_MAGIC      0xDEADBEEF
#define HEAP_GUARD      0xCAFEBABE
#define HEAP_MIN_SIZE   16
#define HEADER_SIZE     sizeof(heap_block_t)
#define GUARD_SIZE      sizeof(uint32_t)

static uint32_t total_allocations = 0;
static uint32_t total_frees = 0;
static uint32_t current_allocations = 0;
static uint32_t bytes_allocated = 0;
static uint32_t peak_allocations = 0;
static uint32_t peak_bytes = 0;

/* Heap state */
static uint32_t heap_start = 0;
static uint32_t heap_end = 0;
static uint32_t heap_max = 0;
static heap_block_t *free_list = NULL;

void heap_init(uint32_t start, uint32_t size)
{
    heap_start = ALIGN_UP(start, 16);
    heap_end = heap_start;
    heap_max = heap_start + size;
    free_list = NULL;
}

static int heap_expand(uint32_t size)
{
    uint32_t new_end = ALIGN_UP(heap_end + size, PAGE_SIZE);
    
    if (new_end > heap_max) {
        return -1; 
    }
    
    for (uint32_t addr = heap_end; addr < new_end; addr += PAGE_SIZE) {
        uint32_t phys = pmm_alloc_frame();
        if (!phys) {
            return -1;
        }
        vmm_map_page(addr, phys, PAGE_KERNEL);
    }
    
    heap_end = new_end;
    return 0;
}

static void set_guard(void *ptr, uint32_t user_size)
{
    uint32_t *guard = (uint32_t *)((uint8_t *)ptr + user_size);
    *guard = HEAP_GUARD;
}

static int check_guard(void *ptr, uint32_t user_size)
{
    uint32_t *guard = (uint32_t *)((uint8_t *)ptr + user_size);
    return *guard == HEAP_GUARD;
}

void *kmalloc(uint32_t size)
{
    if (size == 0) {
        return NULL;
    }
    
    uint32_t user_size = size;
    size = ALIGN_UP(size + GUARD_SIZE, 16);
    if (size < HEAP_MIN_SIZE) {
        size = HEAP_MIN_SIZE;
    }
    
    heap_block_t *block = free_list;
    while (block) {
        if (block->free && block->size >= size) {
            if (block->size >= size + HEADER_SIZE + HEAP_MIN_SIZE) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + HEADER_SIZE + size);
                new_block->size = block->size - size - HEADER_SIZE;
                new_block->magic = HEAP_MAGIC;
                new_block->free = 1;
                new_block->next = block->next;
                new_block->prev = block;
                
                if (block->next) {
                    block->next->prev = new_block;
                }
                block->next = new_block;
                block->size = size;
            }
            
            block->free = 0;
            block->user_size = user_size;
            void *ptr = (void *)((uint8_t *)block + HEADER_SIZE);
            set_guard(ptr, user_size);
            
            total_allocations++;
            current_allocations++;
            bytes_allocated += user_size;
            if (current_allocations > peak_allocations) peak_allocations = current_allocations;
            if (bytes_allocated > peak_bytes) peak_bytes = bytes_allocated;
            
            return ptr;
        }
        block = block->next;
    }
    
    uint32_t needed = HEADER_SIZE + size;
    uint32_t old_end = heap_end;
    if (heap_expand(needed) != 0) {
        return NULL;
    }
    
    block = (heap_block_t *)old_end;
    block->size = size;
    block->user_size = user_size;
    block->magic = HEAP_MAGIC;
    block->free = 0;
    block->next = NULL;
    block->prev = NULL;
    
    if (free_list == NULL) {
        free_list = block;
    } else {
        heap_block_t *last = free_list;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
        block->prev = last;
    }
    
    void *ptr = (void *)((uint8_t *)block + HEADER_SIZE);
    set_guard(ptr, user_size);
    
    total_allocations++;
    current_allocations++;
    bytes_allocated += user_size;
    if (current_allocations > peak_allocations) peak_allocations = current_allocations;
    if (bytes_allocated > peak_bytes) peak_bytes = bytes_allocated;
    
    return ptr;
}

void *kmalloc_aligned(uint32_t size, uint32_t alignment)
{
    uint32_t total = size + alignment + sizeof(void *);
    void *ptr = kmalloc(total);
    if (!ptr) {
        return NULL;
    }
    
    uintptr_t addr = (uintptr_t)ptr + sizeof(void *);
    uintptr_t aligned = ALIGN_UP(addr, alignment);
    
    ((void **)aligned)[-1] = ptr;
    
    return (void *)aligned;
}

int heap_check_overflow(void *ptr)
{
    if (!ptr) return 0;
    
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) return 0;
    
    return check_guard(ptr, block->user_size);
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEADER_SIZE);
    
    if (block->magic != HEAP_MAGIC) {
        return; 
    }
    
    if (!check_guard(ptr, block->user_size)) {
        extern void serial_puts(const char *);
        serial_puts("[HEAP] WARNING: Buffer overflow detected on free!\n");
    }
    
    total_frees++;
    current_allocations--;
    bytes_allocated -= block->user_size;
    
    block->free = 1;
    
    if (block->next && block->next->free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    if (block->prev && block->prev->free) {
        block->prev->size += HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void kfree_aligned(void *ptr)
{
    if (!ptr) {
        return;
    }
    
    void *original = ((void **)ptr)[-1];
    kfree(original);
}

void heap_get_stats(uint32_t *allocs, uint32_t *frees, uint32_t *current, 
                    uint32_t *bytes, uint32_t *peak_allocs, uint32_t *peak_b)
{
    if (allocs) *allocs = total_allocations;
    if (frees) *frees = total_frees;
    if (current) *current = current_allocations;
    if (bytes) *bytes = bytes_allocated;
    if (peak_allocs) *peak_allocs = peak_allocations;
    if (peak_b) *peak_b = peak_bytes;
}

int heap_check_leaks(void)
{
    return (total_allocations != total_frees) ? (total_allocations - total_frees) : 0;
}
