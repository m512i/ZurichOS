/* I/O APIC
 * Handles external device interrupts
 */

#include <kernel/kernel.h>
#include <apic/ioapic.h>
#include <mm/vmm.h>

static volatile uint32_t *ioapic_base = NULL;
static uint32_t ioapic_max_entries = 0;

static inline uint32_t ioapic_read(uint8_t reg)
{
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    return ioapic_base[IOAPIC_REGWIN / 4];
}

static inline void ioapic_write(uint8_t reg, uint32_t value)
{
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    ioapic_base[IOAPIC_REGWIN / 4] = value;
}

void ioapic_init(uint32_t base_addr)
{
    ioapic_base = (volatile uint32_t *)IOAPIC_BASE_VIRT;
    vmm_map_page(IOAPIC_BASE_VIRT, base_addr, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    
    uint32_t version = ioapic_read(IOAPIC_REG_VER);
    ioapic_max_entries = ((version >> 16) & 0xFF) + 1;
    
    for (uint32_t i = 0; i < ioapic_max_entries; i++) {
        ioapic_set_entry(i, IOAPIC_MASKED);
    }
}

void ioapic_set_entry(uint8_t index, uint64_t value)
{
    if (index >= ioapic_max_entries) {
        return;
    }
    
    uint8_t reg = IOAPIC_REG_REDTBL + (index * 2);
    
    ioapic_write(reg, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_write(reg + 1, (uint32_t)(value >> 32));
}

uint64_t ioapic_get_entry(uint8_t index)
{
    if (index >= ioapic_max_entries) {
        return 0;
    }
    
    uint8_t reg = IOAPIC_REG_REDTBL + (index * 2);
    
    uint64_t low = ioapic_read(reg);
    uint64_t high = ioapic_read(reg + 1);
    
    return low | (high << 32);
}

void ioapic_enable_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic)
{
    uint64_t entry = 0;
    
    entry |= vector;                        /* Interrupt vector */
    entry |= (0 << 8);                      /* Delivery mode: Fixed */
    entry |= (0 << 11);                     /* Destination mode: Physical */
    entry |= (0 << 13);                     /* Pin polarity: Active high */
    entry |= (0 << 15);                     /* Trigger mode: Edge */
    entry |= ((uint64_t)dest_apic << 56);   /* Destination APIC ID */
    
    ioapic_set_entry(irq, entry);
}

void ioapic_disable_irq(uint8_t irq)
{
    uint64_t entry = ioapic_get_entry(irq);
    entry |= IOAPIC_MASKED;
    ioapic_set_entry(irq, entry);
}

void ioapic_mask_irq(uint8_t irq)
{
    ioapic_disable_irq(irq);
}

void ioapic_unmask_irq(uint8_t irq)
{
    uint64_t entry = ioapic_get_entry(irq);
    entry &= ~IOAPIC_MASKED;
    ioapic_set_entry(irq, entry);
}

uint32_t ioapic_get_max_entries(void)
{
    return ioapic_max_entries;
}
