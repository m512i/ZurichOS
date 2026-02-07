#ifndef _DRIVERS_PIT_H
#define _DRIVERS_PIT_H

#include <stdint.h>

#define PIT_FREQUENCY   1193182
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

#define PIT_CMD_CHANNEL0    0x00
#define PIT_CMD_CHANNEL1    0x40
#define PIT_CMD_CHANNEL2    0x80
#define PIT_CMD_LATCH       0x00
#define PIT_CMD_LOBYTE      0x10
#define PIT_CMD_HIBYTE      0x20
#define PIT_CMD_LOHIBYTE    0x30
#define PIT_CMD_MODE0       0x00    /* Interrupt on terminal count */
#define PIT_CMD_MODE1       0x02    /* Hardware re-triggerable one-shot */
#define PIT_CMD_MODE2       0x04    /* Rate generator */
#define PIT_CMD_MODE3       0x06    /* Square wave generator */
#define PIT_CMD_BINARY      0x00
#define PIT_CMD_BCD         0x01

void pit_init(uint32_t frequency);
uint64_t pit_get_uptime_ms(void);
uint32_t pit_get_uptime_sec(void);
uint64_t pit_get_ticks(void);
void pit_sleep_ms(uint32_t ms);
uint32_t pit_get_frequency(void);
void pit_use_lapic_timer(void);

#endif
