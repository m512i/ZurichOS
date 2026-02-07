/* VGA Text Mode Driver */

#include <drivers/vga.h>
#include <kernel/kernel.h>

#define VGA_BUFFER  ((uint16_t *)(0xB8000 + KERNEL_VMA))
#define VGA_CTRL_PORT   0x3D4
#define VGA_DATA_PORT   0x3D5

#define SCROLLBACK_LINES    VGA_SCROLLBACK
#define SCROLLBACK_SIZE     (SCROLLBACK_LINES * VGA_WIDTH)

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

static uint16_t scrollback_buffer[SCROLLBACK_SIZE];
static size_t scrollback_pos = 0;      
static size_t scrollback_lines = 0;    
static size_t scroll_offset = 0;       

static uint16_t saved_screen[VGA_WIDTH * VGA_HEIGHT];
static int screen_saved = 0;

void vga_init(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = VGA_BUFFER;
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    vga_set_cursor(0, 0);
    
    scrollback_pos = 0;
    scrollback_lines = 0;
    scroll_offset = 0;
    screen_saved = 0;
}

void vga_setcolor(uint8_t color)
{
    terminal_color = color;
}

void vga_set_cursor(size_t x, size_t y)
{
    uint16_t pos = y * VGA_WIDTH + x;
    
    outb(VGA_CTRL_PORT, 0x0F);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
    outb(VGA_CTRL_PORT, 0x0E);
    outb(VGA_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

static void scrollback_save_line(size_t line)
{
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        scrollback_buffer[scrollback_pos * VGA_WIDTH + x] = 
            terminal_buffer[line * VGA_WIDTH + x];
    }
    scrollback_pos = (scrollback_pos + 1) % SCROLLBACK_LINES;
    if (scrollback_lines < SCROLLBACK_LINES) {
        scrollback_lines++;
    }
}

static void terminal_scroll(void)
{
    scrollback_save_line(0);
    
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = 
                terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = 
            vga_entry(' ', terminal_color);
    }
    
    terminal_row = VGA_HEIGHT - 1;
    scroll_offset = 0;
}

static uint8_t *capture_buffer = NULL;
static uint32_t capture_max = 0;
static uint32_t capture_pos = 0;
static int capturing = 0;

void vga_putchar_at(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void vga_putchar(char c)
{
    if (capturing && capture_buffer && capture_pos < capture_max) {
        capture_buffer[capture_pos++] = (uint8_t)c;
        return;
    }
    
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\t') {
        terminal_column = (terminal_column + 8) & ~7;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            vga_putchar_at(' ', terminal_color, terminal_column, terminal_row);
        }
    } else {
        vga_putchar_at(c, terminal_color, terminal_column, terminal_row);
        terminal_column++;
    }
    
    if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
    
    if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }
    
    vga_set_cursor(terminal_column, terminal_row);
}

void vga_puts(const char *str)
{
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_put_dec(uint32_t num)
{
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    char buf[12];
    int i = 0;
    
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (--i >= 0) {
        vga_putchar(buf[i]);
    }
}

void vga_put_hex(uint32_t num)
{
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        vga_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}

static void save_screen(void)
{
    if (!screen_saved) {
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            saved_screen[i] = terminal_buffer[i];
        }
        screen_saved = 1;
    }
}

static void restore_screen(void)
{
    if (screen_saved) {
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            terminal_buffer[i] = saved_screen[i];
        }
        screen_saved = 0;
    }
    vga_set_cursor(terminal_column, terminal_row);
}

static void draw_scroll_indicator(void)
{
    uint8_t bar_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t thumb_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);

    size_t total = scrollback_lines;
    if (total == 0) return;

    size_t thumb_pos;
    if (total <= VGA_HEIGHT) {
        thumb_pos = 0;
    } else {
        thumb_pos = (scroll_offset * (VGA_HEIGHT - 1)) / total;
        if (thumb_pos >= VGA_HEIGHT) thumb_pos = VGA_HEIGHT - 1;
    }
    size_t thumb_row = (VGA_HEIGHT - 1) - thumb_pos;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        size_t idx = y * VGA_WIDTH + (VGA_WIDTH - 1);
        if (y == thumb_row) {
            terminal_buffer[idx] = vga_entry((char)0xDB, thumb_color);
        } else {
            terminal_buffer[idx] = vga_entry((char)0xB1, bar_color);
        }
    }
}

static void display_scrollback(void)
{
    if (scrollback_lines == 0) return;

    size_t sb_lines = (scroll_offset < VGA_HEIGHT) ? scroll_offset : VGA_HEIGHT;
    size_t live_lines = VGA_HEIGHT - sb_lines;
    size_t start = (scrollback_pos + SCROLLBACK_LINES - scroll_offset) % SCROLLBACK_LINES;

    for (size_t y = 0; y < sb_lines; y++) {
        size_t buf_line = (start + y) % SCROLLBACK_LINES;
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = scrollback_buffer[buf_line * VGA_WIDTH + x];
        }
    }

    if (live_lines > 0 && screen_saved) {
        for (size_t y = 0; y < live_lines; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[(sb_lines + y) * VGA_WIDTH + x] = saved_screen[y * VGA_WIDTH + x];
            }
        }
    } else if (live_lines > 0) {
        for (size_t y = sb_lines; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
            }
        }
    }

    draw_scroll_indicator();

    vga_set_cursor(0, VGA_HEIGHT);
}

void vga_scroll_up(size_t lines)
{
    save_screen();

    size_t new_offset = scroll_offset + lines;
    if (new_offset > scrollback_lines) {
        new_offset = scrollback_lines;
    }

    if (new_offset == scroll_offset) return;
    scroll_offset = new_offset;

    if (scroll_offset == 0) {
        restore_screen();
        return;
    }

    display_scrollback();
}

void vga_scroll_down(size_t lines)
{
    if (scroll_offset == 0) return; 

    if (lines >= scroll_offset) {
        scroll_offset = 0;
        restore_screen();
        return;
    }

    scroll_offset -= lines;
    display_scrollback();
}

int vga_is_scrolled(void)
{
    return scroll_offset > 0;
}

void vga_scroll_reset(void)
{
    if (scroll_offset > 0) {
        scroll_offset = 0;
        restore_screen();
    }
}

uint32_t vga_get_buffer_addr(void)
{
    return (uint32_t)VGA_BUFFER;
}

uint8_t vga_get_color(void)
{
    return terminal_color;
}

void vga_set_capture(uint8_t *buf, uint32_t max)
{
    capture_buffer = buf;
    capture_max = max;
    capture_pos = 0;
    capturing = 1;
}

uint32_t vga_get_capture_len(void)
{
    return capture_pos;
}

void vga_stop_capture(void)
{
    capturing = 0;
}

void vga_puts_ok(void)
{
    uint8_t saved_color = terminal_color;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("OK");
    terminal_color = saved_color;
}

void vga_puts_fail(void)
{
    uint8_t saved_color = terminal_color;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("FAILED");
    terminal_color = saved_color;
}
