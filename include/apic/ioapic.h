#ifndef _APIC_IOAPIC_H
#define _APIC_IOAPIC_H

#include <stdint.h>

#define IOAPIC_BASE_PHYS    0xFEC00000

/* I/O APIC virtual address - dedicated address in kernel MMIO region
 * Use 0xE0001000 (page after LAPIC) */
#define IOAPIC_BASE_VIRT    0xE0001000

#define IOAPIC_REGSEL       0x00    
#define IOAPIC_REGWIN       0x10    

#define IOAPIC_REG_ID       0x00    
#define IOAPIC_REG_VER      0x01    
#define IOAPIC_REG_ARB      0x02    
#define IOAPIC_REG_REDTBL   0x10    /* entries 0-23 */

#define IOAPIC_MASKED       (1ULL << 16)

#define IOAPIC_DELMOD_FIXED     0
#define IOAPIC_DELMOD_LOWPRI    1
#define IOAPIC_DELMOD_SMI       2
#define IOAPIC_DELMOD_NMI       4
#define IOAPIC_DELMOD_INIT      5
#define IOAPIC_DELMOD_EXTINT    7

#define IOAPIC_DESTMOD_PHYSICAL 0
#define IOAPIC_DESTMOD_LOGICAL  1

void ioapic_init(uint32_t base_addr);

void ioapic_set_entry(uint8_t index, uint64_t value);

uint64_t ioapic_get_entry(uint8_t index);

void ioapic_enable_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic);

void ioapic_disable_irq(uint8_t irq);

void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);

uint32_t ioapic_get_max_entries(void);

#endif 
