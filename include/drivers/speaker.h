#ifndef _DRIVERS_SPEAKER_H
#define _DRIVERS_SPEAKER_H

#include <stdint.h>

#define NOTE_C4     262
#define NOTE_D4     294
#define NOTE_E4     330
#define NOTE_F4     349
#define NOTE_G4     392
#define NOTE_A4     440
#define NOTE_B4     494
#define NOTE_C5     523
#define NOTE_D5     587
#define NOTE_E5     659
#define NOTE_F5     698
#define NOTE_G5     784
#define NOTE_A5     880
#define NOTE_B5     988
#define NOTE_C6     1047

void speaker_init(void);
void speaker_play(uint32_t frequency);
void speaker_stop(void);
void speaker_beep(uint32_t frequency, uint32_t duration_ms);
void speaker_beep_default(void);

#endif
