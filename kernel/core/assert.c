/* Kernel Assert Implementation */

#include <kernel/assert.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <drivers/serial.h>

void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
    cli();
    
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_puts("\n");
    vga_puts("================================================================================");
    vga_puts("                         *** ASSERTION FAILED ***                              ");
    vga_puts("================================================================================");
    vga_puts("\n");
    
    vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    vga_puts("Expression: ");
    vga_puts(expr);
    vga_puts("\n");
    
    vga_puts("Location:   ");
    vga_puts(file);
    vga_puts(":");
    vga_put_dec(line);
    vga_puts("\n");
    
    vga_puts("Function:   ");
    vga_puts(func);
    vga_puts("\n");
    
    serial_puts("\n========== ASSERTION FAILED ==========\n");
    serial_puts("Expression: ");
    serial_puts(expr);
    serial_puts("\n");
    serial_puts("File: ");
    serial_puts(file);
    serial_puts("\nLine: ");
    
    char buf[12];
    int i = 0;
    int n = line;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    serial_puts(buf);
    
    serial_puts("\nFunction: ");
    serial_puts(func);
    serial_puts("\n");
    
    vga_puts("\n");
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_puts("System halted. Please reboot.\n");
    
    for (;;) {
        hlt();
    }
}
