/* Global Descriptor Table (GDT)
 * Defines memory segments for protected mode
 */

#include <kernel/kernel.h>
#include <arch/x86/gdt.h>
#include <string.h>

/* IOPB size: 8192 bytes = 65536 ports, 1 bit per port */
#define TSS_IOPB_SIZE 8192

/* Combined TSS + IOPB structure - must be contiguous in memory.
 * The CPU locates the IOPB at tss_base + iomap_base. */
static struct {
    tss_entry_t tss;
    uint8_t     iopb[TSS_IOPB_SIZE];
    uint8_t     iopb_end;   /* Must be 0xFF - marks end of IOPB */
} __attribute__((packed, aligned(16))) tss_block;

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;

extern void gdt_flush(uint32_t gdt_ptr);
extern void tss_flush(void);

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt_entries[num].base_low = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access = access;
}

static void write_tss(int32_t num, uint16_t ss0, uint32_t esp0)
{
    uint32_t base = (uint32_t)&tss_block;
    uint32_t limit = sizeof(tss_block) - 1;

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    memset(&tss_block, 0, sizeof(tss_block));

    tss_block.tss.ss0 = ss0;
    tss_block.tss.esp0 = esp0;
    tss_block.tss.iomap_base = (uint16_t)((uint32_t)&tss_block.iopb - (uint32_t)&tss_block);

    memset(tss_block.iopb, 0xFF, TSS_IOPB_SIZE);
    tss_block.iopb_end = 0xFF;
}

void gdt_init(void)
{
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    /* Null segment */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Ring 0 - Kernel code segment: base=0, limit=4GB */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Ring 0 - Kernel data segment: base=0, limit=4GB */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Ring 1 - Driver code segment: base=0, limit=4GB, DPL=1 */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xBA, 0xCF);  /* 0xBA = 1011 1010 */

    /* Ring 1 - Driver data segment: base=0, limit=4GB, DPL=1 */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xB2, 0xCF);  /* 0xB2 = 1011 0010 */

    /* Ring 2 - Service code segment: base=0, limit=4GB, DPL=2 */
    gdt_set_gate(5, 0, 0xFFFFFFFF, 0xDA, 0xCF);  /* 0xDA = 1101 1010 */

    /* Ring 2 - Service data segment: base=0, limit=4GB, DPL=2 */
    gdt_set_gate(6, 0, 0xFFFFFFFF, 0xD2, 0xCF);  /* 0xD2 = 1101 0010 */

    /* Ring 3 - User code segment: base=0, limit=4GB, DPL=3 */
    gdt_set_gate(7, 0, 0xFFFFFFFF, 0xFA, 0xCF);  /* 0xFA = 1111 1010 */

    /* Ring 3 - User data segment: base=0, limit=4GB, DPL=3 */
    gdt_set_gate(8, 0, 0xFFFFFFFF, 0xF2, 0xCF);  /* 0xF2 = 1111 0010 */
    
    write_tss(9, GDT_KERNEL_DATA_SEGMENT, 0);

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}

void gdt_set_kernel_stack(uint32_t stack)
{
    tss_block.tss.esp0 = stack;
}

void tss_set_ring1_stack(uint32_t esp1, uint16_t ss1)
{
    tss_block.tss.esp1 = esp1;
    tss_block.tss.ss1 = ss1;
}

tss_entry_t *tss_get_entry(void)
{
    return &tss_block.tss;
}

uint8_t *tss_get_iopb(void)
{
    return tss_block.iopb;
}

void tss_set_iopb(const uint8_t *iopb, uint32_t size)
{
    if (size > TSS_IOPB_SIZE) size = TSS_IOPB_SIZE;
    for (uint32_t i = 0; i < size; i++) {
        tss_block.iopb[i] = iopb[i];
    }
}

void tss_clear_iopb(void)
{
    memset(tss_block.iopb, 0x00, TSS_IOPB_SIZE);
}

void tss_deny_all_iopb(void)
{
    memset(tss_block.iopb, 0xFF, TSS_IOPB_SIZE);
}
