#ifndef _DRIVERS_MOUSE_H
#define _DRIVERS_MOUSE_H

#include <stdint.h>

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

#define MOUSE_EVENT_MOVE    0
#define MOUSE_EVENT_PRESS   1
#define MOUSE_EVENT_RELEASE 2
#define MOUSE_EVENT_DRAG    3
#define MOUSE_EVENT_SCROLL  4
#define MOUSE_EVENT_DBLCLICK 5

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;
} mouse_state_t;

typedef struct {
    uint8_t type;
    uint8_t button;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
} mouse_event_t;

#define MOUSE_EVENT_QUEUE_SIZE 64

typedef void (*mouse_event_callback_t)(mouse_event_t *event);

void mouse_init(void);
void mouse_tick(void);
void mouse_set_event_callback(mouse_event_callback_t callback);
mouse_state_t *mouse_get_state(void);
void mouse_set_bounds(int32_t max_x, int32_t max_y);
void mouse_show_cursor(void);
void mouse_hide_cursor(void);
void mouse_update_cursor(void);
int mouse_is_visible(void);
int mouse_get_event(mouse_event_t *event);
void mouse_process_events(void);

int mouse_get_text_col(void);
int mouse_get_text_row(void);

#endif
