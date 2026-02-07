/* PC Speaker Driver
 * Uses PIT Channel 2 to generate square waves
 */

#include <drivers/speaker.h>
#include <drivers/pit.h>
#include <kernel/kernel.h>

#define PIT_FREQUENCY   1193182

#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43
#define SPEAKER_PORT    0x61

void speaker_init(void)
{
}

void speaker_play(uint32_t frequency)
{
    if (frequency == 0) {
        speaker_stop();
        return;
    }
    
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;
    
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));
    
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp | 0x03);
}

void speaker_stop(void)
{
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 1000; j++) {
            inb(0x80);
        }
    }
}

void speaker_beep(uint32_t frequency, uint32_t duration_ms)
{
    speaker_play(frequency);
    delay_ms(duration_ms);
    speaker_stop();
}

void speaker_beep_default(void)
{
    speaker_beep(440, 200);
}
