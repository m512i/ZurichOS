/* Serial Port (UART 16550) Driver */

#include <drivers/serial.h>
#include <kernel/kernel.h>

static uint16_t default_port = COM1_PORT;

void serial_init_port(uint16_t port)
{
    outb(port + UART_IER, 0x00);
    outb(port + UART_LCR, 0x80);
    outb(port + UART_DLL, 0x01);  
    outb(port + UART_DLH, 0x00);
    outb(port + UART_LCR, 0x03);
    outb(port + UART_FCR, 0xC7);
    outb(port + UART_MCR, 0x0B);
    outb(port + UART_MCR, 0x1E);
    outb(port + UART_THR, 0xAE);

    if (inb(port + UART_RBR) != 0xAE) {
        return;
    }
    
    outb(port + UART_MCR, 0x0F);
}

void serial_init(void)
{
    serial_init_port(COM1_PORT);
    default_port = COM1_PORT;
}

int serial_received(uint16_t port)
{
    return inb(port + UART_LSR) & LSR_DATA_READY;
}

char serial_read(uint16_t port)
{
    while (!serial_received(port));
    return inb(port + UART_RBR);
}

int serial_is_transmit_empty(uint16_t port)
{
    return inb(port + UART_LSR) & LSR_THR_EMPTY;
}

void serial_write(uint16_t port, char c)
{
    while (!serial_is_transmit_empty(port));
    outb(port + UART_THR, c);
}

void serial_putc(char c)
{
    if (c == '\n') {
        serial_write(default_port, '\r');
    }
    serial_write(default_port, c);
}

void serial_puts(const char *str)
{
    while (*str) {
        serial_putc(*str++);
    }
}

void e9_putc(char c)
{
    outb(0xE9, c);
}

void e9_puts(const char *str)
{
    while (*str) {
        e9_putc(*str++);
    }
}

static void serial_put_int(int32_t n)
{
    char buf[12];
    int i = 0;
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    if (n == 0) {
        serial_putc('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    if (neg) serial_putc('-');
    
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

static void serial_put_uint(uint32_t n)
{
    char buf[12];
    int i = 0;
    
    if (n == 0) {
        serial_putc('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

static void serial_put_hex(uint32_t n, int uppercase)
{
    const char *hex = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[8];
    int i = 0;
    
    if (n == 0) {
        serial_putc('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = hex[n & 0xF];
        n >>= 4;
    }
    
    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

void serial_printf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                case 'i':
                    serial_put_int(__builtin_va_arg(args, int32_t));
                    break;
                case 'u':
                    serial_put_uint(__builtin_va_arg(args, uint32_t));
                    break;
                case 'x':
                    serial_put_hex(__builtin_va_arg(args, uint32_t), 0);
                    break;
                case 'X':
                    serial_put_hex(__builtin_va_arg(args, uint32_t), 1);
                    break;
                case 'p':
                    serial_puts("0x");
                    serial_put_hex(__builtin_va_arg(args, uint32_t), 0);
                    break;
                case 's': {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (s) {
                        serial_puts(s);
                    } else {
                        serial_puts("(null)");
                    }
                    break;
                }
                case 'c':
                    serial_putc((char)__builtin_va_arg(args, int));
                    break;
                case '%':
                    serial_putc('%');
                    break;
                default:
                    serial_putc('%');
                    serial_putc(*fmt);
                    break;
            }
        } else {
            serial_putc(*fmt);
        }
        fmt++;
    }
    
    __builtin_va_end(args);
}
