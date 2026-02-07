#ifndef _ARCH_X86_GDT_H
#define _ARCH_X86_GDT_H

#include <stdint.h>

/* GDT Segment Selectors */
#define GDT_NULL_SEGMENT        0x00
#define GDT_KERNEL_CODE_SEGMENT 0x08    /* Ring 0 code */
#define GDT_KERNEL_DATA_SEGMENT 0x10    /* Ring 0 data */
#define GDT_DRIVER_CODE_SEGMENT 0x18    /* Ring 1 code (drivers) */
#define GDT_DRIVER_DATA_SEGMENT 0x20    /* Ring 1 data (drivers) */
#define GDT_SERVICE_CODE_SEGMENT 0x28   /* Ring 2 code (services) */
#define GDT_SERVICE_DATA_SEGMENT 0x30   /* Ring 2 data (services) */
#define GDT_USER_CODE_SEGMENT   0x38    /* Ring 3 code (user) */
#define GDT_USER_DATA_SEGMENT   0x40    /* Ring 3 data (user) */
#define GDT_TSS_SEGMENT         0x48

#define GDT_ENTRIES 10

/* Ring levels */
#define RING_KERNEL     0
#define RING_DRIVER     1
#define RING_SERVICE    2
#define RING_USER       3

typedef struct gdt_entry {
    uint16_t limit_low;     
    uint16_t base_low;      
    uint8_t  base_middle;   
    uint8_t  access;        
    uint8_t  granularity;   
    uint8_t  base_high;     
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr {
    uint16_t limit;         
    uint32_t base;          
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss_entry {
    uint32_t prev_tss;      /* Previous TSS (unused) */
    uint32_t esp0;          /* Stack pointer for ring 0 */
    uint32_t ss0;           /* Stack segment for ring 0 */
    uint32_t esp1;          /* Stack pointer for ring 1 (unused) */
    uint32_t ss1;           /* Stack segment for ring 1 (unused) */
    uint32_t esp2;          /* Stack pointer for ring 2 (unused) */
    uint32_t ss2;           /* Stack segment for ring 2 (unused) */
    uint32_t cr3;           /* Page directory */
    uint32_t eip;           /* Instruction pointer */
    uint32_t eflags;        /* Flags register */
    uint32_t eax;           /* General purpose registers */
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;            /* Segment selectors */
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;           
    uint16_t trap;          
    uint16_t iomap_base;    
} __attribute__((packed)) tss_entry_t;

void gdt_init(void);
void gdt_set_kernel_stack(uint32_t stack);
void tss_set_ring1_stack(uint32_t esp1, uint16_t ss1);
tss_entry_t *tss_get_entry(void);
uint8_t *tss_get_iopb(void);
void tss_set_iopb(const uint8_t *iopb, uint32_t size);
void tss_clear_iopb(void);
void tss_deny_all_iopb(void);

#endif
