#ifndef _DRIVERS_VGA_H
#define _DRIVERS_VGA_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH           80
#define VGA_HEIGHT          25
#define VGA_SCROLLBACK      200


enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | (bg << 4);
}

static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_init(void);
void vga_clear(void);
void vga_setcolor(uint8_t color);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_putchar_at(char c, uint8_t color, size_t x, size_t y);
void vga_set_cursor(size_t x, size_t y);
void vga_put_dec(uint32_t num);
void vga_put_hex(uint32_t num);
void vga_scroll_up(size_t lines);
void vga_scroll_down(size_t lines);
int vga_is_scrolled(void);
void vga_scroll_reset(void);
uint32_t vga_get_buffer_addr(void);
void vga_puts_ok(void);
void vga_puts_fail(void);
uint8_t vga_get_color(void);

#endif
