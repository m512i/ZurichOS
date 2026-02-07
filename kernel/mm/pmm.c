/* Physical Memory Manager (PMM)
 * Manages physical page frame allocation using a bitmap
 */

#include <kernel/kernel.h>
#include <kernel/multiboot.h>
#include <mm/pmm.h>

static uint32_t *pmm_bitmap = NULL;
static uint32_t pmm_bitmap_size = 0;     
static uint32_t pmm_total_frames = 0;
static uint32_t pmm_used_frames = 0;

static uint32_t pmm_total_memory = 0;     

#define BITMAP_INDEX(frame)     ((frame) / 32)
#define BITMAP_OFFSET(frame)    ((frame) % 32)
#define BITMAP_SET(frame)       (pmm_bitmap[BITMAP_INDEX(frame)] |= (1 << BITMAP_OFFSET(frame)))
#define BITMAP_CLEAR(frame)     (pmm_bitmap[BITMAP_INDEX(frame)] &= ~(1 << BITMAP_OFFSET(frame)))
#define BITMAP_TEST(frame)      (pmm_bitmap[BITMAP_INDEX(frame)] & (1 << BITMAP_OFFSET(frame)))

extern uint32_t _kernel_end_phys;

void pmm_init(multiboot_info_t *mboot)
{
    if (mboot->flags & MULTIBOOT_INFO_MEMORY) {
        pmm_total_memory = (mboot->mem_upper + 1024) * 1024;
    } else {
        pmm_total_memory = 256 * 1024 * 1024;
    }
    
    if (pmm_total_memory == 0) {
        pmm_total_memory = 256 * 1024 * 1024;
    }
    
    pmm_total_frames = pmm_total_memory / PAGE_SIZE;
    pmm_bitmap_size = (pmm_total_frames + 31) / 32;
    pmm_bitmap = (uint32_t *)((uint32_t)&_kernel_end_phys + KERNEL_VMA);
    
    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    pmm_used_frames = pmm_total_frames;
    
    for (uint32_t frame = 256; frame < pmm_total_frames; frame++) {
        BITMAP_CLEAR(frame);
        pmm_used_frames--;
    }
    
    uint32_t kernel_end = (uint32_t)&_kernel_end_phys + (pmm_bitmap_size * sizeof(uint32_t));
    kernel_end = ALIGN_UP(kernel_end, PAGE_SIZE);
    
    for (uint32_t addr = 0x100000; addr < kernel_end; addr += PAGE_SIZE) {
        pmm_mark_used(addr);
    }
}

uint32_t pmm_alloc_frame(void)
{
    if (pmm_used_frames >= pmm_total_frames) {
        return 0;
    }
    
    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t frame = i * 32 + j;
                if (frame >= pmm_total_frames) {
                    return 0;
                }
                if (!BITMAP_TEST(frame)) {
                    BITMAP_SET(frame);
                    pmm_used_frames++;
                    return frame * PAGE_SIZE;
                }
            }
        }
    }
    
    return 0;
}

void pmm_free_frame(uint32_t addr)
{
    uint32_t frame = addr / PAGE_SIZE;
    if (frame >= pmm_total_frames) {
        return;
    }
    
    if (BITMAP_TEST(frame)) {
        BITMAP_CLEAR(frame);
        pmm_used_frames--;
    }
}

void pmm_mark_used(uint32_t addr)
{
    uint32_t frame = addr / PAGE_SIZE;
    if (frame >= pmm_total_frames) {
        return;
    }
    
    if (!BITMAP_TEST(frame)) {
        BITMAP_SET(frame);
        pmm_used_frames++;
    }
}

uint32_t pmm_get_total_memory(void)
{
    return pmm_total_memory;
}

uint32_t pmm_get_used_memory(void)
{
    return pmm_used_frames * PAGE_SIZE;
}

uint32_t pmm_get_free_memory(void)
{
    return (pmm_total_frames - pmm_used_frames) * PAGE_SIZE;
}
