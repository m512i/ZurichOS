#ifndef _DRIVERS_FRAMEBUFFER_H
#define _DRIVERS_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>


#define FB_RGB(r, g, b)         (0xFF000000 | ((r) << 16) | ((g) << 8) | (b))
#define FB_RGBA(r, g, b, a)     (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))


#define FB_COLOR_BLACK          FB_RGB(0, 0, 0)
#define FB_COLOR_WHITE          FB_RGB(255, 255, 255)
#define FB_COLOR_RED            FB_RGB(255, 0, 0)
#define FB_COLOR_GREEN          FB_RGB(0, 255, 0)
#define FB_COLOR_BLUE           FB_RGB(0, 0, 255)
#define FB_COLOR_CYAN           FB_RGB(0, 255, 255)
#define FB_COLOR_MAGENTA        FB_RGB(255, 0, 255)
#define FB_COLOR_YELLOW         FB_RGB(255, 255, 0)
#define FB_COLOR_DARK_GREY      FB_RGB(64, 64, 64)
#define FB_COLOR_LIGHT_GREY     FB_RGB(192, 192, 192)
#define FB_COLOR_ORANGE         FB_RGB(255, 165, 0)


#define FB_TERM_BG              FB_RGB(30, 30, 46)
#define FB_TERM_FG              FB_RGB(205, 214, 244)
#define FB_TERM_CURSOR          FB_RGB(245, 224, 220)
#define FB_TERM_GREEN           FB_RGB(166, 227, 161)
#define FB_TERM_RED             FB_RGB(243, 139, 168)
#define FB_TERM_YELLOW          FB_RGB(249, 226, 175)
#define FB_TERM_BLUE            FB_RGB(137, 180, 250)
#define FB_TERM_MAGENTA         FB_RGB(203, 166, 247)
#define FB_TERM_CYAN            FB_RGB(148, 226, 213)


typedef struct {
    uint32_t *addr;             
    uint32_t phys_addr;         
    uint32_t width;             
    uint32_t height;            
    uint32_t pitch;             
    uint8_t bpp;                
    uint32_t size;              

    
    uint32_t *back_buffer;      

    
    uint32_t text_cols;         
    uint32_t text_rows;         
    uint32_t cursor_x;          
    uint32_t cursor_y;          
    uint32_t fg_color;          
    uint32_t bg_color;          
    int cursor_visible;         
} framebuffer_t;


int fb_init(uint32_t phys_addr, uint32_t width, uint32_t height,
            uint32_t pitch, uint8_t bpp);
int fb_init_bga(uint32_t width, uint32_t height);
int fb_is_available(void);
framebuffer_t *fb_get_info(void);


void fb_swap_buffers(void);
void fb_flush(void);
void fb_clear(uint32_t color);


static inline void fb_put_pixel(uint32_t *buf, uint32_t pitch,
                                 int x, int y, uint32_t color)
{
    buf[y * (pitch >> 2) + x] = color;
}


void fb_draw_hline(int x, int y, int width, uint32_t color);
void fb_draw_vline(int x, int y, int height, uint32_t color);
void fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);


void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);


void fb_console_init(void);
void fb_console_clear(void);
void fb_console_putchar(char c);
void fb_console_puts(const char *str);
void fb_console_set_color(uint32_t fg, uint32_t bg);
void fb_console_put_dec(uint32_t num);
void fb_console_put_hex(uint32_t num);
void fb_console_puts_ok(void);
void fb_console_puts_fail(void);
uint32_t fb_console_get_cols(void);
uint32_t fb_console_get_rows(void);
void fb_console_begin_batch(void);
void fb_console_end_batch(void);
void fb_console_scroll_up(uint32_t lines);
void fb_console_scroll_down(uint32_t lines);
int fb_console_is_scrolled(void);
void fb_console_scroll_reset(void);


#define FB_FONT_WIDTH   8
#define FB_FONT_HEIGHT  16

#endif
