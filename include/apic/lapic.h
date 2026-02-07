#ifndef _APIC_LAPIC_H
#define _APIC_LAPIC_H

#include <stdint.h>

#define LAPIC_BASE_PHYS     0xFEE00000

/* LAPIC virtual address - dedicated address in kernel MMIO region
 * Can't use 0xFEE00000 directly as it conflicts with recursive page tables
 * Use 0xE0000000 region for device MMIO */
#define LAPIC_BASE_VIRT     0xE0000000

#define LAPIC_TIMER_VECTOR  32
#define LAPIC_REG_ID            0x020   
#define LAPIC_REG_VERSION       0x030   
#define LAPIC_REG_TPR           0x080   
#define LAPIC_REG_APR           0x090   
#define LAPIC_REG_PPR           0x0A0   
#define LAPIC_REG_EOI           0x0B0   
#define LAPIC_REG_RRD           0x0C0   
#define LAPIC_REG_LDR           0x0D0   
#define LAPIC_REG_DFR           0x0E0   
#define LAPIC_REG_SPURIOUS      0x0F0   
#define LAPIC_REG_ISR_BASE      0x100   
#define LAPIC_REG_TMR_BASE      0x180   
#define LAPIC_REG_IRR_BASE      0x200   
#define LAPIC_REG_ESR           0x280   
#define LAPIC_REG_LVT_CMCI      0x2F0   
#define LAPIC_REG_ICR_LOW       0x300   
#define LAPIC_REG_ICR_HIGH      0x310   
#define LAPIC_REG_LVT_TIMER     0x320   
#define LAPIC_REG_LVT_THERMAL   0x330   
#define LAPIC_REG_LVT_PERF      0x340   
#define LAPIC_REG_LVT_LINT0     0x350   
#define LAPIC_REG_LVT_LINT1     0x360   
#define LAPIC_REG_LVT_ERROR     0x370   
#define LAPIC_REG_TIMER_INIT    0x380   
#define LAPIC_REG_TIMER_CURR    0x390   
#define LAPIC_REG_TIMER_DIV     0x3E0   

void lapic_init(uint32_t base_addr);
void lapic_eoi(void);
uint32_t lapic_get_id(void);
void lapic_send_ipi(uint32_t apic_id, uint32_t vector);
void lapic_timer_init(uint32_t frequency);
void lapic_timer_stop(void);
void lapic_timer_handler(void);
uint32_t lapic_get_ticks(void);
uint64_t lapic_get_uptime_ms(void);
uint32_t lapic_get_uptime_sec(void);
uint32_t lapic_get_frequency(void);
int lapic_is_enabled(void);

#endif