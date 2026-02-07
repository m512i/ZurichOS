#include <drivers/mouse.h>
#include <drivers/framebuffer.h>
#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <arch/x86/idt.h>
#include <string.h>

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_STATUS_OUTPUT 0x01
#define MOUSE_STATUS_INPUT  0x02

#define CURSOR_W 12
#define CURSOR_H 19

static const uint8_t cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0},
    {1,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};

static mouse_state_t state;
static mouse_event_callback_t mouse_event_cb = NULL;
static int32_t max_x = 1024;
static int32_t max_y = 768;

static uint8_t mouse_cycle = 0;
static int8_t mouse_bytes[4];
static uint8_t prev_buttons = 0;
static int has_wheel = 0;
static uint8_t packet_size = 3;

static volatile uint32_t tick_count = 0;
static uint32_t last_click_tick = 0;
static int32_t last_click_x = 0;
static int32_t last_click_y = 0;

void mouse_tick(void)
{
    tick_count++;
}

static int cursor_visible = 0;
static int cursor_drawn = 0;
static int32_t drawn_x = 0;
static int32_t drawn_y = 0;
static uint32_t cursor_save[CURSOR_W * CURSOR_H];

static mouse_event_t event_queue[MOUSE_EVENT_QUEUE_SIZE];
static volatile uint32_t eq_head = 0;
static volatile uint32_t eq_tail = 0;

static void event_enqueue(mouse_event_t *ev)
{
    uint32_t next = (eq_head + 1) % MOUSE_EVENT_QUEUE_SIZE;
    if (next == eq_tail) return;
    event_queue[eq_head] = *ev;
    eq_head = next;
}

static void mouse_save_under(int32_t x, int32_t y);
static void mouse_restore_under(int32_t x, int32_t y);
static void mouse_draw_cursor(int32_t x, int32_t y);

static void mouse_vram_pre(void)
{
    if (cursor_drawn) {
        mouse_restore_under(drawn_x, drawn_y);
        cursor_drawn = 0;
    }
}

static void mouse_vram_post(void)
{
    if (cursor_visible) {
        mouse_save_under(state.x, state.y);
        mouse_draw_cursor(state.x, state.y);
        drawn_x = state.x;
        drawn_y = state.y;
        cursor_drawn = 1;
    }
}

static void mouse_wait_write(void)
{
    int timeout = 100000;
    while (timeout--) {
        if (!(inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_INPUT))
            return;
    }
}

static void mouse_wait_read(void)
{
    int timeout = 100000;
    while (timeout--) {
        if (inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_OUTPUT)
            return;
    }
}

static void mouse_write(uint8_t data)
{
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, data);
}

static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(MOUSE_DATA_PORT);
}

static void mouse_save_under(int32_t x, int32_t y)
{
    if (!fb_is_available()) return;
    framebuffer_t *info = fb_get_info();
    if (!info || !info->back_buffer) return;

    uint32_t stride = info->pitch >> 2;
    uint32_t *buf = info->back_buffer;
    int idx = 0;

    for (int dy = 0; dy < CURSOR_H; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)info->height) {
            idx += CURSOR_W;
            continue;
        }
        for (int dx = 0; dx < CURSOR_W; dx++) {
            int px = x + dx;
            if (px >= 0 && px < (int)info->width) {
                cursor_save[idx] = buf[py * stride + px];
            } else {
                cursor_save[idx] = 0;
            }
            idx++;
        }
    }
}

static void mouse_restore_under(int32_t x, int32_t y)
{
    if (!fb_is_available()) return;
    framebuffer_t *info = fb_get_info();
    if (!info || !info->addr) return;

    uint32_t stride = info->pitch >> 2;
    uint32_t *vram = info->addr;
    int idx = 0;

    for (int dy = 0; dy < CURSOR_H; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)info->height) {
            idx += CURSOR_W;
            continue;
        }
        for (int dx = 0; dx < CURSOR_W; dx++) {
            int px = x + dx;
            if (px >= 0 && px < (int)info->width) {
                vram[py * stride + px] = cursor_save[idx];
            }
            idx++;
        }
    }
}

static void mouse_draw_cursor(int32_t x, int32_t y)
{
    if (!fb_is_available()) return;
    framebuffer_t *info = fb_get_info();
    if (!info || !info->addr) return;

    uint32_t stride = info->pitch >> 2;
    uint32_t *vram = info->addr;

    for (int dy = 0; dy < CURSOR_H; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)info->height) continue;
        for (int dx = 0; dx < CURSOR_W; dx++) {
            int px = x + dx;
            if (px < 0 || px >= (int)info->width) continue;

            uint8_t pixel = cursor_bitmap[dy][dx];
            if (pixel == 1) {
                vram[py * stride + px] = 0xFF000000;
            } else if (pixel == 2) {
                vram[py * stride + px] = 0xFFFFFFFF;
            }
        }
    }
}

static void mouse_process_packet(void)
{
    int32_t dx = mouse_bytes[1];
    int32_t dy = -mouse_bytes[2];
    uint8_t new_buttons = (uint8_t)mouse_bytes[0] & 0x07;
    int8_t scroll = has_wheel ? mouse_bytes[3] : 0;

    state.x += dx;
    state.y += dy;

    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x >= max_x) state.x = max_x - 1;
    if (state.y >= max_y) state.y = max_y - 1;

    uint8_t pressed = new_buttons & ~prev_buttons;
    uint8_t released = prev_buttons & ~new_buttons;

    for (int b = 0; b < 3; b++) {
        uint8_t mask = (uint8_t)(1 << b);
        if (pressed & mask) {
            mouse_event_t ev;
            ev.type = MOUSE_EVENT_PRESS;
            ev.button = mask;
            ev.x = state.x;
            ev.y = state.y;
            ev.dx = dx;
            ev.dy = dy;
            ev.buttons = new_buttons;
            event_enqueue(&ev);

            if (mask == MOUSE_BUTTON_LEFT) {
                uint32_t now = tick_count;
                int32_t cdx = state.x - last_click_x;
                int32_t cdy = state.y - last_click_y;
                if (cdx < 0) cdx = -cdx;
                if (cdy < 0) cdy = -cdy;

                if ((now - last_click_tick) < 30 && cdx < 8 && cdy < 16) {
                    mouse_event_t dev;
                    dev.type = MOUSE_EVENT_DBLCLICK;
                    dev.button = MOUSE_BUTTON_LEFT;
                    dev.x = state.x;
                    dev.y = state.y;
                    dev.dx = 0;
                    dev.dy = 0;
                    dev.buttons = new_buttons;
                    event_enqueue(&dev);
                    last_click_tick = 0;
                } else {
                    last_click_tick = now;
                    last_click_x = state.x;
                    last_click_y = state.y;
                }
            }
        }
        if (released & mask) {
            mouse_event_t ev;
            ev.type = MOUSE_EVENT_RELEASE;
            ev.button = mask;
            ev.x = state.x;
            ev.y = state.y;
            ev.dx = dx;
            ev.dy = dy;
            ev.buttons = new_buttons;
            event_enqueue(&ev);
        }
    }

    if (scroll) {
        mouse_event_t ev;
        ev.type = MOUSE_EVENT_SCROLL;
        ev.button = 0;
        ev.x = state.x;
        ev.y = state.y;
        ev.dx = 0;
        ev.dy = (int32_t)scroll;
        ev.buttons = new_buttons;
        event_enqueue(&ev);
    }

    if ((dx || dy) && new_buttons) {
        mouse_event_t ev;
        ev.type = MOUSE_EVENT_DRAG;
        ev.button = new_buttons;
        ev.x = state.x;
        ev.y = state.y;
        ev.dx = dx;
        ev.dy = dy;
        ev.buttons = new_buttons;
        event_enqueue(&ev);
    } else if ((dx || dy) && !new_buttons) {
        mouse_event_t ev;
        ev.type = MOUSE_EVENT_MOVE;
        ev.button = 0;
        ev.x = state.x;
        ev.y = state.y;
        ev.dx = dx;
        ev.dy = dy;
        ev.buttons = 0;
        event_enqueue(&ev);
    }

    state.buttons = new_buttons;
    prev_buttons = new_buttons;

    if (cursor_visible) {
        if (cursor_drawn) {
            mouse_restore_under(drawn_x, drawn_y);
        }

        mouse_save_under(state.x, state.y);
        mouse_draw_cursor(state.x, state.y);

        drawn_x = state.x;
        drawn_y = state.y;
        cursor_drawn = 1;
    }
}

static void mouse_handler(registers_t *regs)
{
    (void)regs;

    uint8_t status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x01) || !(status & 0x20)) {
        return;
    }

    uint8_t data = inb(MOUSE_DATA_PORT);

    if (mouse_cycle == 0) {
        if (!(data & 0x08)) {
            return;
        }
        mouse_bytes[0] = (int8_t)data;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = (int8_t)data;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_bytes[2] = (int8_t)data;
        if (packet_size == 4) {
            mouse_cycle = 3;
        } else {
            mouse_cycle = 0;
            mouse_process_packet();
        }
    } else if (mouse_cycle == 3) {
        mouse_bytes[3] = (int8_t)data;
        mouse_cycle = 0;
        mouse_process_packet();
    }
}

void mouse_init(void)
{
    state.x = max_x / 2;
    state.y = max_y / 2;
    state.buttons = 0;
    mouse_cycle = 0;

    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xA8);

    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait_read();
    uint8_t status = inb(MOUSE_DATA_PORT);
    status |= 0x02;
    status &= ~0x20;
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, status);

    mouse_write(0xFF);
    mouse_read();
    mouse_read();
    mouse_read();

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();

    mouse_write(0xF2);
    mouse_read();
    uint8_t mouse_id = mouse_read();
    if (mouse_id == 3) {
        has_wheel = 1;
        packet_size = 4;
        serial_puts("[MOUSE] Scroll wheel detected (IntelliMouse)\n");
    } else {
        has_wheel = 0;
        packet_size = 3;
    }

    mouse_write(0xF4);
    mouse_read();

    register_interrupt_handler(IRQ12, mouse_handler);

    if (fb_is_available()) {
        framebuffer_t *info = fb_get_info();
        if (info) {
            max_x = (int32_t)info->width;
            max_y = (int32_t)info->height;
            state.x = max_x / 2;
            state.y = max_y / 2;
        }
        fb_set_vram_hooks(mouse_vram_pre, mouse_vram_post);
    }

    serial_puts("[MOUSE] PS/2 mouse initialized\n");
}

void mouse_set_event_callback(mouse_event_callback_t callback)
{
    mouse_event_cb = callback;
}

mouse_state_t *mouse_get_state(void)
{
    return &state;
}

void mouse_set_bounds(int32_t mx, int32_t my)
{
    max_x = mx;
    max_y = my;
}

int mouse_get_event(mouse_event_t *event)
{
    if (eq_head == eq_tail) return 0;
    *event = event_queue[eq_tail];
    eq_tail = (eq_tail + 1) % MOUSE_EVENT_QUEUE_SIZE;
    return 1;
}

void mouse_process_events(void)
{
    mouse_event_t ev;
    while (mouse_get_event(&ev)) {
        if (mouse_event_cb) {
            mouse_event_cb(&ev);
        }
    }
}

int mouse_get_text_col(void)
{
    return (int)(state.x / 8);
}

int mouse_get_text_row(void)
{
    return (int)(state.y / 16);
}

void mouse_show_cursor(void)
{
    cursor_visible = 1;
    if (!cursor_drawn && fb_is_available()) {
        mouse_save_under(state.x, state.y);
        mouse_draw_cursor(state.x, state.y);
        drawn_x = state.x;
        drawn_y = state.y;
        cursor_drawn = 1;
    }
}

void mouse_hide_cursor(void)
{
    if (cursor_drawn) {
        mouse_restore_under(drawn_x, drawn_y);
        cursor_drawn = 0;
    }
    cursor_visible = 0;
}

void mouse_update_cursor(void)
{
    if (!cursor_visible || !fb_is_available()) return;

    if (cursor_drawn) {
        mouse_restore_under(drawn_x, drawn_y);
    }

    mouse_save_under(state.x, state.y);
    mouse_draw_cursor(state.x, state.y);
    drawn_x = state.x;
    drawn_y = state.y;
    cursor_drawn = 1;
}

int mouse_is_visible(void)
{
    return cursor_visible;
}
