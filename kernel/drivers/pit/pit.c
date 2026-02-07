/* PIT Driver
 * Provides system timing and uptime tracking
 * Falls back to PIT when APIC is not available
 */

#include <drivers/pit.h>
#include <arch/x86/idt.h>
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include <apic/lapic.h>

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_freq = 0;

static void pit_handler(registers_t *regs)
{
    (void)regs;
    pit_ticks++;
    scheduler_tick();
}

static void lapic_timer_irq_handler(registers_t *regs)
{
    (void)regs;
    lapic_timer_handler();
    scheduler_tick();
}

void pit_init(uint32_t frequency)
{
    uint32_t divisor = PIT_FREQUENCY / frequency;

    if (divisor < 1) divisor = 1;
    if (divisor > 65535) divisor = 65535;
    
    pit_freq = PIT_FREQUENCY / divisor;
    
    register_interrupt_handler(32, pit_handler);
    
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_LOHIBYTE | PIT_CMD_MODE3 | PIT_CMD_BINARY);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));         
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));  
    
    pit_ticks = 0;
}

void pit_use_lapic_timer(void)
{
    register_interrupt_handler(32, lapic_timer_irq_handler);
}

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}

uint64_t pit_get_uptime_ms(void)
{
    if (pit_freq == 0) return 0;
    return (pit_ticks * 1000) / pit_freq;
}

uint32_t pit_get_uptime_sec(void)
{
    if (pit_freq == 0) return 0;
    return (uint32_t)(pit_ticks / pit_freq);
}

uint32_t pit_get_frequency(void)
{
    return pit_freq;
}

void pit_sleep_ms(uint32_t ms)
{
    uint64_t target = pit_ticks + (ms * pit_freq) / 1000;
    while (pit_ticks < target) {
        hlt();  
    }
}
