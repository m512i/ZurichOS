/* Local APIC
 * Per-CPU interrupt controller
 */

#include <kernel/kernel.h>
#include <apic/lapic.h>
#include <mm/vmm.h>

static volatile uint32_t *lapic_base = NULL;

static inline uint32_t lapic_read(uint32_t reg)
{
    return lapic_base[reg / 4];
}

static inline void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_base[reg / 4] = value;
}

void lapic_init(uint32_t base_addr)
{
    lapic_base = (volatile uint32_t *)(LAPIC_BASE_VIRT);
    vmm_map_page(LAPIC_BASE_VIRT, base_addr, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    lapic_write(LAPIC_REG_SPURIOUS, 0x1FF);
    lapic_write(LAPIC_REG_TPR, 0);
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_eoi();
}

void lapic_eoi(void)
{
    if (lapic_base) {
        lapic_write(LAPIC_REG_EOI, 0);
    }
}

uint32_t lapic_get_id(void)
{
    if (!lapic_base) {
        return 0;
    }
    return (lapic_read(LAPIC_REG_ID) >> 24) & 0xFF;
}

void lapic_send_ipi(uint32_t apic_id, uint32_t vector)
{
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1 << 12)) {
        __asm__ volatile("pause");
    }
    
    lapic_write(LAPIC_REG_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, vector);
}

static volatile uint64_t lapic_ticks = 0;
static uint32_t lapic_ticks_per_second = 0;
static uint32_t lapic_timer_freq = 0;

void lapic_timer_handler(void)
{
    lapic_ticks++;
}

uint64_t lapic_get_uptime_ms(void)
{
    if (lapic_timer_freq == 0) return 0;
    return (lapic_ticks * 1000) / lapic_timer_freq;
}

uint32_t lapic_get_uptime_sec(void)
{
    if (lapic_timer_freq == 0) return 0;
    return (uint32_t)(lapic_ticks / lapic_timer_freq);
}

uint32_t lapic_get_frequency(void)
{
    return lapic_timer_freq;
}

uint32_t lapic_get_ticks(void)
{
    return lapic_ticks;
}

void lapic_timer_init(uint32_t frequency)
{
    if (!lapic_base) return;
    
    lapic_write(LAPIC_REG_TIMER_DIV, 0x03);
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000);  /* Masked */
    lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFF);

    /* PIT channel 0 at 1193182 Hz, 10ms = 11932 ticks */
    outb(0x43, 0x30);  /* Channel 0, lobyte/hibyte, mode 0 */
    outb(0x40, 0x9C);  /* Low byte of 11932 */
    outb(0x40, 0x2E);  /* High byte of 11932 */
    
    uint8_t status;
    do {
        outb(0x43, 0xE2);
        status = inb(0x40);
    } while (!(status & 0x80));
    
    uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CURR);
    lapic_write(LAPIC_REG_TIMER_INIT, 0);
    
    lapic_ticks_per_second = elapsed * 100;
    
    uint32_t count = lapic_ticks_per_second / frequency;
    if (count == 0) count = 1;
    
    lapic_timer_freq = frequency;
    lapic_ticks = 0;
    
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | 0x20000);  /* Periodic */
    lapic_write(LAPIC_REG_TIMER_INIT, count);
}

void lapic_timer_stop(void)
{
    lapic_write(LAPIC_REG_TIMER_INIT, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000);
}

int lapic_is_enabled(void)
{
    return lapic_base != NULL;
}
