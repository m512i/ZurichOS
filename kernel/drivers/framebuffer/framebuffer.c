/* Framebuffer Graphics Driver
 * Linear framebuffer with double buffering, drawing primitives, and text console
 */

#include <drivers/framebuffer.h>
#include <drivers/fb_font.h>
#include <drivers/serial.h>
#include <drivers/pci.h>
#include <kernel/kernel.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <string.h>

static framebuffer_t fb;
static int fb_available = 0;
static int fb_swap_deferred = 0;
static volatile int fb_dirty = 0;


#define FB_SCROLLBACK_LINES 1000
#define FB_MAX_COLS         128

typedef struct {
    char ch;
    uint32_t fg;
    uint32_t bg;
} fb_cell_t;

static fb_cell_t (*fb_scrollback)[FB_MAX_COLS] = NULL;  
static uint32_t fb_sb_write = 0;       
static uint32_t fb_sb_count = 0;       
static uint32_t fb_sb_offset = 0;      


static fb_cell_t fb_screen[48][FB_MAX_COLS];  


static int fb_dirty_x0, fb_dirty_y0, fb_dirty_x1, fb_dirty_y1;
static int fb_dirty_full = 0;  

static void fb_dirty_reset(void)
{
    fb_dirty_x0 = 0x7FFFFFFF;
    fb_dirty_y0 = 0x7FFFFFFF;
    fb_dirty_x1 = 0;
    fb_dirty_y1 = 0;
    fb_dirty_full = 0;
}

static void fb_dirty_mark(int x, int y, int w, int h)
{
    if (x < fb_dirty_x0) fb_dirty_x0 = x;
    if (y < fb_dirty_y0) fb_dirty_y0 = y;
    if (x + w > fb_dirty_x1) fb_dirty_x1 = x + w;
    if (y + h > fb_dirty_y1) fb_dirty_y1 = y + h;
}

static void fb_dirty_mark_full(void)
{
    fb_dirty_full = 1;
}


#define FB_VIRT_BASE    0xE0100000


#define BGA_INDEX_PORT      0x01CE
#define BGA_DATA_PORT       0x01CF

#define BGA_REG_ID          0x00
#define BGA_REG_XRES        0x01
#define BGA_REG_YRES        0x02
#define BGA_REG_BPP         0x03
#define BGA_REG_ENABLE      0x04
#define BGA_REG_BANK        0x05
#define BGA_REG_VIRT_WIDTH  0x06
#define BGA_REG_VIRT_HEIGHT 0x07
#define BGA_REG_X_OFFSET    0x08
#define BGA_REG_Y_OFFSET    0x09

#define BGA_DISABLED        0x00
#define BGA_ENABLED         0x01
#define BGA_LFB_ENABLED     0x40

#define BGA_ID_MIN          0xB0C0  
#define BGA_ID_MAX          0xB0C5  

static void bga_write(uint16_t reg, uint16_t val)
{
    outw(BGA_INDEX_PORT, reg);
    outw(BGA_DATA_PORT, val);
}

static uint16_t bga_read(uint16_t reg)
{
    outw(BGA_INDEX_PORT, reg);
    return inw(BGA_DATA_PORT);
}



int fb_init(uint32_t phys_addr, uint32_t width, uint32_t height,
            uint32_t pitch, uint8_t bpp)
{
    if (bpp != 32) {
        serial_puts("[FB] ERROR: Only 32bpp supported\n");
        return -1;
    }

    fb.phys_addr = phys_addr;
    fb.width = width;
    fb.height = height;
    fb.pitch = pitch;
    fb.bpp = bpp;
    fb.size = pitch * height;

    serial_puts("[FB] Mapping framebuffer: phys=");
    serial_printf("0x%x", phys_addr);
    serial_printf(" size=0x%x", fb.size);
    serial_printf(" %dx%d\n", width, height);

    
    uint32_t pages = (fb.size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = FB_VIRT_BASE + (i * PAGE_SIZE);
        uint32_t phys = phys_addr + (i * PAGE_SIZE);
        vmm_map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_PWT | PAGE_PCD);
    }

    fb.addr = (uint32_t *)FB_VIRT_BASE;

    
    fb.back_buffer = (uint32_t *)kmalloc(fb.size);
    if (!fb.back_buffer) {
        serial_puts("[FB] ERROR: Failed to allocate back buffer\n");
        
        fb.back_buffer = fb.addr;
    }

    
    fb.text_cols = width / FB_FONT_WIDTH;
    fb.text_rows = height / FB_FONT_HEIGHT;
    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb.fg_color = FB_TERM_FG;
    fb.bg_color = FB_TERM_BG;
    fb.cursor_visible = 1;

    fb_available = 1;

    
    fb_scrollback = (fb_cell_t (*)[FB_MAX_COLS])kmalloc(
        FB_SCROLLBACK_LINES * FB_MAX_COLS * sizeof(fb_cell_t));
    if (!fb_scrollback) {
        serial_puts("[FB] WARNING: Failed to allocate scrollback\n");
    } else {
        memset(fb_scrollback, 0, FB_SCROLLBACK_LINES * FB_MAX_COLS * sizeof(fb_cell_t));
    }

    
    fb_dirty_reset();

    
    fb_clear(fb.bg_color);
    fb_swap_buffers();

    serial_printf("[FB] Console: %dx%d chars\n", fb.text_cols, fb.text_rows);
    serial_puts("[FB] Framebuffer initialized\n");

    return 0;
}

int fb_init_bga(uint32_t width, uint32_t height)
{
    
    uint16_t bga_id = bga_read(BGA_REG_ID);
    serial_printf("[FB] BGA ID: 0x%x\n", bga_id);

    if (bga_id < BGA_ID_MIN || bga_id > BGA_ID_MAX) {
        serial_puts("[FB] BGA device not found\n");
        return -1;
    }

    
    pci_device_t *vga_dev = pci_find_device_by_class(PCI_CLASS_DISPLAY, 0x00);
    if (!vga_dev) {
        serial_puts("[FB] No VGA PCI device found\n");
        return -1;
    }

    uint32_t fb_phys = vga_dev->bar[0] & 0xFFFFFFF0;  
    serial_printf("[FB] VGA PCI: vendor=0x%x device=0x%x BAR0=0x%x\n",
                  vga_dev->vendor_id, vga_dev->device_id, fb_phys);

    if (fb_phys == 0) {
        serial_puts("[FB] BAR0 is zero, cannot map framebuffer\n");
        return -1;
    }

    
    bga_write(BGA_REG_ENABLE, BGA_DISABLED);
    bga_write(BGA_REG_XRES, (uint16_t)width);
    bga_write(BGA_REG_YRES, (uint16_t)height);
    bga_write(BGA_REG_BPP, 32);
    bga_write(BGA_REG_VIRT_WIDTH, (uint16_t)width);
    bga_write(BGA_REG_VIRT_HEIGHT, (uint16_t)height);
    bga_write(BGA_REG_X_OFFSET, 0);
    bga_write(BGA_REG_Y_OFFSET, 0);
    bga_write(BGA_REG_ENABLE, BGA_ENABLED | BGA_LFB_ENABLED);

    serial_printf("[FB] BGA mode set: %dx%dx32\n", width, height);

    
    uint32_t pitch = width * 4;

    return fb_init(fb_phys, width, height, pitch, 32);
}

int fb_is_available(void)
{
    return fb_available;
}

framebuffer_t *fb_get_info(void)
{
    return &fb;
}



void fb_swap_buffers(void)
{
    if (!fb_available) return;

    if (fb.back_buffer != fb.addr) {
        
        uint32_t *src = fb.back_buffer;
        uint32_t *dst = fb.addr;
        uint32_t dwords = fb.size >> 2;
        __asm__ volatile(
            "cld\n"
            "rep movsl\n"
            : "+S"(src), "+D"(dst), "+c"(dwords)
            :
            : "memory"
        );
    }
}

void fb_flush(void)
{
    if (!fb_available || !fb_dirty) return;
    fb_dirty = 0;

    if (fb.back_buffer == fb.addr) return;

    if (fb_dirty_full) {
        
        fb_swap_buffers();
        fb_dirty_reset();
        return;
    }

    
    if (fb_dirty_x0 >= fb_dirty_x1 || fb_dirty_y0 >= fb_dirty_y1) {
        fb_dirty_reset();
        return;
    }
    if (fb_dirty_x1 > (int)fb.width) fb_dirty_x1 = (int)fb.width;
    if (fb_dirty_y1 > (int)fb.height) fb_dirty_y1 = (int)fb.height;
    if (fb_dirty_x0 < 0) fb_dirty_x0 = 0;
    if (fb_dirty_y0 < 0) fb_dirty_y0 = 0;

    
    uint32_t pitch_dwords = fb.pitch >> 2;
    for (int y = fb_dirty_y0; y < fb_dirty_y1; y++) {
        uint32_t *src = fb.back_buffer + y * pitch_dwords + fb_dirty_x0;
        uint32_t *dst = fb.addr + y * pitch_dwords + fb_dirty_x0;
        uint32_t count = (uint32_t)(fb_dirty_x1 - fb_dirty_x0);
        memcpy(dst, src, count * 4);
    }

    fb_dirty_reset();
}

void fb_clear(uint32_t color)
{
    if (!fb_available) return;

    uint32_t *buf = fb.back_buffer;
    uint32_t pixels = fb.size >> 2;

    
    __asm__ volatile(
        "cld\n"
        "rep stosl\n"
        : "+D"(buf), "+c"(pixels)
        : "a"(color)
        : "memory"
    );
    fb_dirty_mark_full();
}



void fb_draw_hline(int x, int y, int width, uint32_t color)
{
    if (!fb_available) return;
    if (y < 0 || y >= (int)fb.height) return;
    if (x < 0) { width += x; x = 0; }
    if (x + width > (int)fb.width) width = (int)fb.width - x;
    if (width <= 0) return;

    uint32_t *ptr = &fb.back_buffer[y * (fb.pitch >> 2) + x];
    for (int i = 0; i < width; i++) {
        *ptr++ = color;
    }
    fb_dirty_mark(x, y, width, 1);
}

void fb_draw_vline(int x, int y, int height, uint32_t color)
{
    if (!fb_available) return;
    if (x < 0 || x >= (int)fb.width) return;
    if (y < 0) { height += y; y = 0; }
    if (y + height > (int)fb.height) height = (int)fb.height - y;
    if (height <= 0) return;

    uint32_t stride = fb.pitch >> 2;
    uint32_t *ptr = &fb.back_buffer[y * stride + x];
    for (int i = 0; i < height; i++) {
        *ptr = color;
        ptr += stride;
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!fb_available) return;
    fb_draw_hline(x, y, w, color);
    fb_draw_hline(x, y + h - 1, w, color);
    fb_draw_vline(x, y, h, color);
    fb_draw_vline(x + w - 1, y, h, color);
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!fb_available) return;
    for (int i = 0; i < h; i++) {
        fb_draw_hline(x, y + i, w, color);
    }
    fb_dirty_mark(x, y, w, h);
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    if (!fb_available) return;

    
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int err = dx - dy;
    uint32_t stride = fb.pitch >> 2;

    while (1) {
        if (x0 >= 0 && x0 < (int)fb.width && y0 >= 0 && y0 < (int)fb.height) {
            fb.back_buffer[y0 * stride + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}



void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg)
{
    if (!fb_available) return;

    uint8_t idx = (uint8_t)c;
    if (idx >= FONT_CHARS) idx = 0;

    const uint8_t *glyph = fb_font_data[idx];
    uint32_t stride = fb.pitch >> 2;

    for (int dy = 0; dy < FONT_HEIGHT; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)fb.height) continue;

        uint8_t row = glyph[dy];
        uint32_t *line = &fb.back_buffer[py * stride + x];

        
        if (x >= 0 && x + 7 < (int)fb.width) {
            line[0] = (row & 0x80) ? fg : bg;
            line[1] = (row & 0x40) ? fg : bg;
            line[2] = (row & 0x20) ? fg : bg;
            line[3] = (row & 0x10) ? fg : bg;
            line[4] = (row & 0x08) ? fg : bg;
            line[5] = (row & 0x04) ? fg : bg;
            line[6] = (row & 0x02) ? fg : bg;
            line[7] = (row & 0x01) ? fg : bg;
        } else {
            
            for (int dx = 0; dx < FONT_WIDTH; dx++) {
                int px = x + dx;
                if (px >= 0 && px < (int)fb.width) {
                    fb.back_buffer[py * stride + px] =
                        (row & (0x80 >> dx)) ? fg : bg;
                }
            }
        }
    }
    fb_dirty_mark(x, y, FONT_WIDTH, FONT_HEIGHT);
}

void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg)
{
    if (!fb_available) return;

    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += FONT_HEIGHT;
        } else {
            fb_draw_char(cx, y, *str, fg, bg);
            cx += FONT_WIDTH;
        }
        str++;
    }
}



static void fb_console_scroll(void)
{
    if (!fb_available) return;

    
    if (fb_scrollback) {
        memcpy(fb_scrollback[fb_sb_write], fb_screen[0],
               fb.text_cols * sizeof(fb_cell_t));
        fb_sb_write = (fb_sb_write + 1) % FB_SCROLLBACK_LINES;
        if (fb_sb_count < FB_SCROLLBACK_LINES)
            fb_sb_count++;
    }

    
    for (uint32_t r = 0; r < fb.text_rows - 1; r++)
        memcpy(fb_screen[r], fb_screen[r + 1],
               fb.text_cols * sizeof(fb_cell_t));

    
    for (uint32_t c = 0; c < fb.text_cols; c++) {
        fb_screen[fb.text_rows - 1][c].ch = ' ';
        fb_screen[fb.text_rows - 1][c].fg = fb.fg_color;
        fb_screen[fb.text_rows - 1][c].bg = fb.bg_color;
    }

    
    uint32_t row_bytes = fb.pitch * FONT_HEIGHT;
    uint32_t total_text_bytes = fb.pitch * (fb.text_rows * FONT_HEIGHT);

    uint8_t *buf = (uint8_t *)fb.back_buffer;
    memmove(buf, buf + row_bytes, total_text_bytes - row_bytes);

    
    uint32_t last_row_y = (fb.text_rows - 1) * FONT_HEIGHT;
    fb_fill_rect(0, last_row_y, fb.width, FONT_HEIGHT, fb.bg_color);
    fb_dirty_mark_full();
}

static void fb_console_draw_cursor(uint32_t color)
{
    if (!fb_available || !fb.cursor_visible) return;

    int x = fb.cursor_x * FB_FONT_WIDTH;
    int y = fb.cursor_y * FB_FONT_HEIGHT + FB_FONT_HEIGHT - 2;

    
    fb_draw_hline(x, y, FB_FONT_WIDTH, color);
    fb_draw_hline(x, y + 1, FB_FONT_WIDTH, color);
}

void fb_console_init(void)
{
    if (!fb_available) return;

    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb.fg_color = FB_TERM_FG;
    fb.bg_color = FB_TERM_BG;
    fb.cursor_visible = 1;

    fb_clear(fb.bg_color);
    fb_swap_buffers();
}

void fb_console_clear(void)
{
    if (!fb_available) return;

    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb_sb_offset = 0;

    
    for (uint32_t r = 0; r < fb.text_rows; r++) {
        for (uint32_t c = 0; c < fb.text_cols; c++) {
            fb_screen[r][c].ch = ' ';
            fb_screen[r][c].fg = fb.fg_color;
            fb_screen[r][c].bg = fb.bg_color;
        }
    }

    fb_clear(fb.bg_color);
    fb_swap_buffers();
}

static void fb_console_redraw(void)
{
    if (!fb_available) return;

    fb_clear(fb.bg_color);
    for (uint32_t r = 0; r < fb.text_rows; r++) {
        for (uint32_t c = 0; c < fb.text_cols; c++) {
            fb_cell_t *cell = &fb_screen[r][c];
            if (cell->ch > ' ')
                fb_draw_char(c * FB_FONT_WIDTH, r * FB_FONT_HEIGHT,
                             cell->ch, cell->fg, cell->bg);
        }
    }
    fb_console_draw_cursor(FB_TERM_CURSOR);
    fb_swap_buffers();
}

void fb_console_putchar(char c)
{
    if (!fb_available) return;

    
    if (fb_sb_offset > 0) {
        fb_sb_offset = 0;
        fb_console_redraw();
    }

    
    fb_console_draw_cursor(fb.bg_color);

    if (c == '\n') {
        fb.cursor_x = 0;
        fb.cursor_y++;
    } else if (c == '\r') {
        fb.cursor_x = 0;
    } else if (c == '\t') {
        fb.cursor_x = (fb.cursor_x + 8) & ~7;
        if (fb.cursor_x >= fb.text_cols) {
            fb.cursor_x = 0;
            fb.cursor_y++;
        }
    } else if (c == '\b') {
        if (fb.cursor_x > 0) {
            fb.cursor_x--;
            fb_screen[fb.cursor_y][fb.cursor_x].ch = ' ';
            fb_screen[fb.cursor_y][fb.cursor_x].fg = fb.fg_color;
            fb_screen[fb.cursor_y][fb.cursor_x].bg = fb.bg_color;
            fb_draw_char(fb.cursor_x * FB_FONT_WIDTH,
                         fb.cursor_y * FB_FONT_HEIGHT,
                         ' ', fb.fg_color, fb.bg_color);
        }
    } else {
        fb_screen[fb.cursor_y][fb.cursor_x].ch = c;
        fb_screen[fb.cursor_y][fb.cursor_x].fg = fb.fg_color;
        fb_screen[fb.cursor_y][fb.cursor_x].bg = fb.bg_color;
        fb_draw_char(fb.cursor_x * FB_FONT_WIDTH,
                     fb.cursor_y * FB_FONT_HEIGHT,
                     c, fb.fg_color, fb.bg_color);
        fb.cursor_x++;
    }

    
    if (fb.cursor_x >= fb.text_cols) {
        fb.cursor_x = 0;
        fb.cursor_y++;
    }

    
    while (fb.cursor_y >= fb.text_rows) {
        fb_console_scroll();
        fb.cursor_y--;
    }

    
    fb_console_draw_cursor(FB_TERM_CURSOR);

    
    fb_dirty = 1;
}

void fb_console_puts(const char *str)
{
    if (!fb_available) return;

    
    if (fb_sb_offset > 0) {
        fb_sb_offset = 0;
        fb_console_redraw();
    }

    
    fb_console_draw_cursor(fb.bg_color);

    while (*str) {
        char c = *str++;

        if (c == '\n') {
            fb.cursor_x = 0;
            fb.cursor_y++;
        } else if (c == '\r') {
            fb.cursor_x = 0;
        } else if (c == '\t') {
            fb.cursor_x = (fb.cursor_x + 8) & ~7;
            if (fb.cursor_x >= fb.text_cols) {
                fb.cursor_x = 0;
                fb.cursor_y++;
            }
        } else if (c == '\b') {
            if (fb.cursor_x > 0) {
                fb.cursor_x--;
                fb_screen[fb.cursor_y][fb.cursor_x].ch = ' ';
                fb_screen[fb.cursor_y][fb.cursor_x].fg = fb.fg_color;
                fb_screen[fb.cursor_y][fb.cursor_x].bg = fb.bg_color;
                fb_draw_char(fb.cursor_x * FB_FONT_WIDTH,
                             fb.cursor_y * FB_FONT_HEIGHT,
                             ' ', fb.fg_color, fb.bg_color);
            }
        } else {
            fb_screen[fb.cursor_y][fb.cursor_x].ch = c;
            fb_screen[fb.cursor_y][fb.cursor_x].fg = fb.fg_color;
            fb_screen[fb.cursor_y][fb.cursor_x].bg = fb.bg_color;
            fb_draw_char(fb.cursor_x * FB_FONT_WIDTH,
                         fb.cursor_y * FB_FONT_HEIGHT,
                         c, fb.fg_color, fb.bg_color);
            fb.cursor_x++;
        }

        if (fb.cursor_x >= fb.text_cols) {
            fb.cursor_x = 0;
            fb.cursor_y++;
        }

        while (fb.cursor_y >= fb.text_rows) {
            fb_console_scroll();
            fb.cursor_y--;
        }
    }

    
    fb_console_draw_cursor(FB_TERM_CURSOR);

    
    fb_swap_buffers();
}

void fb_console_set_color(uint32_t fg, uint32_t bg)
{
    fb.fg_color = fg;
    fb.bg_color = bg;
}

void fb_console_put_dec(uint32_t num)
{
    if (num == 0) {
        fb_console_putchar('0');
        return;
    }

    char buf[12];
    int i = 0;

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (--i >= 0) {
        fb_console_putchar(buf[i]);
    }
}

void fb_console_put_hex(uint32_t num)
{
    fb_console_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        fb_console_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}

void fb_console_puts_ok(void)
{
    uint32_t saved_fg = fb.fg_color;
    fb.fg_color = FB_TERM_GREEN;
    fb_console_puts("OK");
    fb.fg_color = saved_fg;
}

void fb_console_puts_fail(void)
{
    uint32_t saved_fg = fb.fg_color;
    fb.fg_color = FB_TERM_RED;
    fb_console_puts("FAILED");
    fb.fg_color = saved_fg;
}

uint32_t fb_console_get_cols(void)
{
    return fb.text_cols;
}

uint32_t fb_console_get_rows(void)
{
    return fb.text_rows;
}

void fb_console_begin_batch(void)
{
    fb_swap_deferred++;
}

void fb_console_end_batch(void)
{
    if (fb_swap_deferred > 0)
        fb_swap_deferred--;
    if (fb_swap_deferred == 0 && fb_available)
        fb_swap_buffers();
}



static void fb_console_render_scrollback(void)
{
    if (!fb_available) return;

    fb_clear(fb.bg_color);

    /* The view is a window of text_rows lines.
     * Total logical lines = sb_count (scrollback) + text_rows (live screen).
     * The live view shows the last text_rows lines (offset 0).
     * sb_offset=N means we shift the window up by N lines.
     *
     * For each screen row r (0..text_rows-1), the logical line is:
     *   logical = (sb_count + text_rows - 1) - sb_offset - (text_rows - 1 - r)
     *           = sb_count - sb_offset + r
     *
     * If logical < sb_count  -> it's scrollback line #logical
     * If logical >= sb_count -> it's live screen row (logical - sb_count)
     */

    
    for (uint32_t r = 0; r < fb.text_rows; r++) {
        int logical = (int)fb_sb_count - (int)fb_sb_offset + (int)r;

        if (logical < 0) {
            
            continue;
        } else if (logical < (int)fb_sb_count) {
            
            int idx = ((int)fb_sb_write - (int)fb_sb_count + logical + FB_SCROLLBACK_LINES) % FB_SCROLLBACK_LINES;
            for (uint32_t c = 0; c < fb.text_cols; c++) {
                fb_cell_t *cell = &fb_scrollback[idx][c];
                if (cell->ch > ' ')
                    fb_draw_char(c * FB_FONT_WIDTH, r * FB_FONT_HEIGHT,
                                 cell->ch, cell->fg, cell->bg);
            }
        } else {
            
            int screen_row = logical - (int)fb_sb_count;
            if (screen_row >= 0 && screen_row < (int)fb.text_rows) {
                for (uint32_t c = 0; c < fb.text_cols; c++) {
                    fb_cell_t *cell = &fb_screen[screen_row][c];
                    if (cell->ch > ' ')
                        fb_draw_char(c * FB_FONT_WIDTH, r * FB_FONT_HEIGHT,
                                     cell->ch, cell->fg, cell->bg);
                }
            }
        }
    }

    
    if (fb_sb_offset > 0) {
        const char *ind = "[SCROLL]";
        int x = ((int)fb.text_cols - 8) * FB_FONT_WIDTH;
        while (*ind) {
            fb_draw_char(x, 0, *ind, FB_TERM_YELLOW, FB_TERM_BG);
            x += FB_FONT_WIDTH;
            ind++;
        }
    }

    fb_swap_buffers();
}

void fb_console_scroll_up(uint32_t lines)
{
    if (!fb_available || fb_sb_count == 0) return;

    fb_sb_offset += lines;
    if (fb_sb_offset > fb_sb_count)
        fb_sb_offset = fb_sb_count;

    fb_console_render_scrollback();
}

void fb_console_scroll_down(uint32_t lines)
{
    if (!fb_available || fb_sb_offset == 0) return;

    if (lines >= fb_sb_offset) {
        fb_sb_offset = 0;
        fb_console_redraw();
        return;
    }

    fb_sb_offset -= lines;
    fb_console_render_scrollback();
}

int fb_console_is_scrolled(void)
{
    return fb_sb_offset > 0;
}

void fb_console_scroll_reset(void)
{
    if (fb_sb_offset > 0) {
        fb_sb_offset = 0;
        fb_console_redraw();
    }
}
