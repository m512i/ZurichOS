/* PS/2 Keyboard Driver
 * Improved version with:
 * - Ring buffer for key events (IRQ-safe)
 * - Proper status port checking
 * - Extended key handling for make AND break
 * - Key event structure with modifiers
 */

#include <kernel/kernel.h>
#include <drivers/keyboard.h>
#include <arch/x86/idt.h>
#include <drivers/vga.h>
#include <drivers/framebuffer.h>

/* I/O ports */
#define KBD_DATA_PORT       0x60
#define KBD_STATUS_PORT     0x64
#define KBD_COMMAND_PORT    0x64

/* status register bits */
#define KBD_STATUS_OUTPUT   0x01
#define KBD_STATUS_INPUT    0x02

/* Special scancodes */
#define SCANCODE_EXTENDED   0xE0
#define SCANCODE_EXTENDED2  0xE1  /* Pause/Break */
#define SCANCODE_ACK        0xFA
#define SCANCODE_RESEND     0xFE
#define SCANCODE_ERROR      0x00
#define SCANCODE_ERROR2     0xFF

/* Extended key scancodes (after 0xE0) */
#define SCANCODE_PGUP       0x49
#define SCANCODE_PGDN       0x51
#define SCANCODE_HOME       0x47
#define SCANCODE_END        0x4F
#define SCANCODE_UP         0x48
#define SCANCODE_DOWN       0x50
#define SCANCODE_LEFT       0x4B
#define SCANCODE_RIGHT      0x4D
#define SCANCODE_INSERT     0x52
#define SCANCODE_DELETE     0x53

/* Ring buffer for key events */
#define EVENT_BUFFER_SIZE   64

static key_event_t event_buffer[EVENT_BUFFER_SIZE];
static volatile uint32_t event_head = 0;  /* Write position (IRQ) */
static volatile uint32_t event_tail = 0;  /* Read position (main) */
static int extended_key = 0;
static int e1_state = 0;
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int caps_lock = 0;
static int right_ctrl = 0;
static int right_alt = 0;

static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_to_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static keyboard_callback_t keyboard_callback = NULL;
static keyboard_event_callback_t keyboard_event_callback = NULL;

static void buffer_add_event(key_event_t *event)
{
    uint32_t next = (event_head + 1) % EVENT_BUFFER_SIZE;
    if (next != event_tail) {
        event_buffer[event_head] = *event;
        event_head = next;
    }
}

static uint8_t get_modifiers(void)
{
    uint8_t mods = 0;
    if (shift_pressed) mods |= KEY_MOD_SHIFT;
    if (ctrl_pressed || right_ctrl) mods |= KEY_MOD_CTRL;
    if (alt_pressed || right_alt) mods |= KEY_MOD_ALT;
    if (caps_lock) mods |= KEY_MOD_CAPS;
    return mods;
}

static char scancode_to_char(uint8_t scancode, bool extended)
{
    if (extended) return 0;
    if (scancode >= 128) return 0;
    
    int use_shift = shift_pressed;
    
    if (caps_lock) {
        char c = scancode_to_ascii[scancode];
        if (c >= 'a' && c <= 'z') {
            use_shift = !use_shift;
        }
    }
    
    return use_shift ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
}

static void process_scancode(uint8_t scancode)
{
    if (e1_state > 0) {
        e1_state--;
        return;
    }
    
    switch (scancode) {
        case SCANCODE_EXTENDED:
            extended_key = 1;
            return;
        case SCANCODE_EXTENDED2:
            e1_state = 5;
            return;
        case SCANCODE_ACK:
        case SCANCODE_RESEND:
        case SCANCODE_ERROR:
        case SCANCODE_ERROR2:
            return;
    }
    
    bool is_extended = extended_key;
    extended_key = 0;
    
    bool released = (scancode & 0x80) != 0;
    uint8_t code = scancode & 0x7F;
    
    if (is_extended) {
        switch (code) {
            case 0x1D: right_ctrl = !released; break;
            case 0x38: right_alt = !released; break;
        }
    } else {
        switch (code) {
            case 0x2A:
            case 0x36:
                shift_pressed = !released;
                break;
            case 0x1D:
                ctrl_pressed = !released;
                break;
            case 0x38:
                alt_pressed = !released;
                break;
            case 0x3A:
                if (!released) caps_lock = !caps_lock;
                break;
        }
    }
    
    key_event_t event;
    event.scancode = code;
    event.pressed = !released;
    event.extended = is_extended;
    event.modifiers = get_modifiers();
    event.ascii = released ? 0 : scancode_to_char(code, is_extended);
    
    buffer_add_event(&event);
}

static void keyboard_handler(registers_t *regs)
{
    (void)regs;

    int max_reads = 16;
    while (max_reads-- > 0) {
        uint8_t status = inb(KBD_STATUS_PORT);
        if (!(status & KBD_STATUS_OUTPUT)) {
            break;
        }
        
        uint8_t scancode = inb(KBD_DATA_PORT);
        process_scancode(scancode);
    }
}

void keyboard_process_events(void)
{
    key_event_t event;
    
    while (keyboard_get_event(&event)) {
        if (event.pressed && event.extended) {
            int is_shift = (event.modifiers & KEY_MOD_SHIFT) != 0;
            int use_fb = fb_is_available();

            switch (event.scancode) {
                case SCANCODE_PGUP:
                    if (use_fb)
                        fb_console_scroll_up(is_shift ? (fb_console_get_rows() / 2) : (fb_console_get_rows() - 1));
                    else
                        vga_scroll_up(is_shift ? (VGA_HEIGHT / 2) : (VGA_HEIGHT - 1));
                    continue;
                case SCANCODE_PGDN:
                    if (use_fb)
                        fb_console_scroll_down(is_shift ? (fb_console_get_rows() / 2) : (fb_console_get_rows() - 1));
                    else
                        vga_scroll_down(is_shift ? (VGA_HEIGHT / 2) : (VGA_HEIGHT - 1));
                    continue;
                case SCANCODE_HOME:
                    if (use_fb)
                        fb_console_scroll_up(200);
                    else
                        vga_scroll_up(VGA_SCROLLBACK);
                    continue;
                case SCANCODE_END:
                    if (use_fb)
                        fb_console_scroll_reset();
                    else
                        vga_scroll_reset();
                    continue;
                case SCANCODE_UP:
                    if (is_shift) {
                        if (use_fb)
                            fb_console_scroll_up(1);
                        else
                            vga_scroll_up(1);
                        continue;
                    }
                    if (keyboard_callback) {
                        keyboard_callback('\x1B');
                        keyboard_callback('[');
                        keyboard_callback('A');
                    }
                    continue;
                case SCANCODE_DOWN:
                    if (is_shift) {
                        if (use_fb)
                            fb_console_scroll_down(1);
                        else
                            vga_scroll_down(1);
                        continue;
                    }
                    if (keyboard_callback) {
                        keyboard_callback('\x1B');
                        keyboard_callback('[');
                        keyboard_callback('B');
                    }
                    continue;
                case SCANCODE_LEFT:
                    if (keyboard_callback) {
                        keyboard_callback('\x1B');
                        keyboard_callback('[');
                        keyboard_callback('D');
                    }
                    continue;
                case SCANCODE_RIGHT:
                    if (keyboard_callback) {
                        keyboard_callback('\x1B');
                        keyboard_callback('[');
                        keyboard_callback('C');
                    }
                    continue;
            }
        }
        
        if (event.pressed && event.ascii) {
            if (fb_is_available() && fb_console_is_scrolled())
                fb_console_scroll_reset();
            else if (!fb_is_available() && vga_is_scrolled())
                vga_scroll_reset();
        }
        
        if (keyboard_event_callback) {
            keyboard_event_callback(&event);
        }
        
        if (event.pressed && (event.modifiers & KEY_MOD_CTRL) && keyboard_callback) {
            char ctrl_char = 0;
            if (event.ascii >= 'a' && event.ascii <= 'z') {
                ctrl_char = event.ascii - 'a' + 1;
            } else if (event.ascii >= 'A' && event.ascii <= 'Z') {
                ctrl_char = event.ascii - 'A' + 1;
            }
            if (ctrl_char) {
                keyboard_callback(ctrl_char);
                continue;
            }
        }
        
        if (event.pressed && event.ascii && keyboard_callback) {
            keyboard_callback(event.ascii);
        }
    }
}

void keyboard_init(void)
{
    event_head = 0;
    event_tail = 0;
    
    /* IRQ1 = interrupt 33 */
    register_interrupt_handler(IRQ1, keyboard_handler);
}

void keyboard_set_callback(keyboard_callback_t callback)
{
    keyboard_callback = callback;
}

void keyboard_set_event_callback(keyboard_event_callback_t callback)
{
    keyboard_event_callback = callback;
}

bool keyboard_has_event(void)
{
    return event_head != event_tail;
}

bool keyboard_get_event(key_event_t *event)
{
    if (event_head == event_tail) {
        return false;
    }
    
    *event = event_buffer[event_tail];
    event_tail = (event_tail + 1) % EVENT_BUFFER_SIZE;
    return true;
}

int keyboard_is_shift_pressed(void)
{
    return shift_pressed;
}

int keyboard_is_ctrl_pressed(void)
{
    return ctrl_pressed || right_ctrl;
}

int keyboard_is_alt_pressed(void)
{
    return alt_pressed || right_alt;
}

int keyboard_is_caps_lock(void)
{
    return caps_lock;
}
