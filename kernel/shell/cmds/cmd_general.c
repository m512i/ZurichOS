/* Shell Commands - General Category
 * help, clear, echo, version, uptime, color, exit, halt, reboot
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <drivers/pit.h>
#include <arch/x86/idt.h>
#include <apic/lapic.h>
#include <string.h>

void cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_clear();
}

void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) vga_putchar(' ');
        vga_puts(argv[i]);
    }
    vga_putchar('\n');
}

void cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("ZurichOS v0.1.0\n");
    vga_puts("Built: " __DATE__ " " __TIME__ "\n");
    vga_puts("Architecture: i686 (x86 32-bit)\n");
}

void cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t uptime_sec;
    uint32_t ticks;
    uint32_t freq;
    
    if (idt_is_apic_mode() && lapic_is_enabled()) {
        uptime_sec = lapic_get_uptime_sec();
        ticks = lapic_get_ticks();
        freq = lapic_get_frequency();
    } else {
        uptime_sec = pit_get_uptime_sec();
        ticks = (uint32_t)pit_get_ticks();
        freq = pit_get_frequency();
    }
    
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    
    vga_puts("Uptime: ");
    vga_put_dec(hours);
    vga_puts("h ");
    vga_put_dec(minutes);
    vga_puts("m ");
    vga_put_dec(seconds);
    vga_puts("s\n");
    
    vga_puts("Ticks: ");
    vga_put_dec(ticks);
    vga_puts(" (");
    vga_put_dec(freq);
    vga_puts(" Hz)\n");
}

void cmd_color(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: color <0-15>\n");
        vga_puts("Colors: 0=black, 1=blue, 2=green, 3=cyan,\n");
        vga_puts("        4=red, 5=magenta, 6=brown, 7=light grey,\n");
        vga_puts("        8=dark grey, 9=light blue, 10=light green,\n");
        vga_puts("        11=light cyan, 12=light red, 13=light magenta,\n");
        vga_puts("        14=yellow, 15=white\n");
        return;
    }
    
    uint8_t color = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9') {
        color = color * 10 + (*p - '0');
        p++;
    }
    
    if (color > 15) color = 15;
    vga_setcolor(vga_entry_color(color, VGA_COLOR_BLACK));
    vga_puts("Color changed.\n");
}

void cmd_exit(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Goodbye!\n");
    vga_puts("System halted.\n");
    
    cli();
    for (;;) {
        hlt();
    }
}

void cmd_halt(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("System halted.\n");
    cli();
    for (;;) hlt();
}

void cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Rebooting...\n");
    
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    
    hlt();
}
