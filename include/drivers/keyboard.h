#ifndef _DRIVERS_KEYBOARD_H
#define _DRIVERS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t scancode;       
    bool pressed;           
    bool extended;          
    uint8_t modifiers;      
    char ascii;             
} key_event_t;

#define KEY_MOD_SHIFT   0x01
#define KEY_MOD_CTRL    0x02
#define KEY_MOD_ALT     0x04
#define KEY_MOD_CAPS    0x08

typedef void (*keyboard_callback_t)(char c);
typedef void (*keyboard_event_callback_t)(key_event_t *event);

void keyboard_init(void);
void keyboard_set_callback(keyboard_callback_t callback);
void keyboard_set_event_callback(keyboard_event_callback_t callback);
bool keyboard_has_event(void);
bool keyboard_get_event(key_event_t *event);
void keyboard_process_events(void);
int keyboard_is_shift_pressed(void);
int keyboard_is_ctrl_pressed(void);
int keyboard_is_alt_pressed(void);
int keyboard_is_caps_lock(void);

#endif 
