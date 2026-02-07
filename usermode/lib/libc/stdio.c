/* User-space stdio implementation */

#define SYS_WRITE  2

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

static void write_str(const char *str)
{
    int len = 0;
    while (str[len]) len++;
    syscall3(SYS_WRITE, 1, (int)str, len);
}

static void write_char(char c)
{
    syscall3(SYS_WRITE, 1, (int)&c, 1);
}

static void write_int(int n)
{
    char buf[12];
    int i = 0;
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    if (n == 0) {
        write_char('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    if (neg) write_char('-');
    
    while (i > 0) {
        write_char(buf[--i]);
    }
}

static void write_uint(unsigned int n)
{
    char buf[12];
    int i = 0;
    
    if (n == 0) {
        write_char('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    while (i > 0) {
        write_char(buf[--i]);
    }
}

static void write_hex(unsigned int n, int uppercase)
{
    const char *hex = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[8];
    int i = 0;
    
    if (n == 0) {
        write_char('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = hex[n & 0xF];
        n >>= 4;
    }
    
    while (i > 0) {
        write_char(buf[--i]);
    }
}

void printf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                case 'i':
                    write_int(__builtin_va_arg(args, int));
                    break;
                case 'u':
                    write_uint(__builtin_va_arg(args, unsigned int));
                    break;
                case 'x':
                    write_hex(__builtin_va_arg(args, unsigned int), 0);
                    break;
                case 'X':
                    write_hex(__builtin_va_arg(args, unsigned int), 1);
                    break;
                case 'p':
                    write_str("0x");
                    write_hex(__builtin_va_arg(args, unsigned int), 0);
                    break;
                case 's': {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (s) {
                        write_str(s);
                    } else {
                        write_str("(null)");
                    }
                    break;
                }
                case 'c':
                    write_char((char)__builtin_va_arg(args, int));
                    break;
                case '%':
                    write_char('%');
                    break;
                default:
                    write_char('%');
                    write_char(*fmt);
                    break;
            }
        } else {
            write_char(*fmt);
        }
        fmt++;
    }
    
    __builtin_va_end(args);
}

int puts(const char *str)
{
    write_str(str);
    write_char('\n');
    return 0;
}

int putchar(int c)
{
    write_char((char)c);
    return c;
}
