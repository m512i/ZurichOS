/* Counter program - counts from 1 to 10 */

#define SYS_EXIT   0
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

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
        : "memory"
    );
    return ret;
}

static void print(const char *str)
{
    int len = 0;
    while (str[len]) len++;
    syscall3(SYS_WRITE, 1, (int)str, len);
}

static void print_num(int n)
{
    char buf[12];
    int i = 0;
    
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    /* Reverse */
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    buf[i] = '\0';
    print(buf);
}

void _start(void)
{
    print("Counting from 1 to 10:\n");
    
    for (int i = 1; i <= 10; i++) {
        print("  ");
        print_num(i);
        print("\n");
    }
    
    print("Done!\n");
    syscall1(SYS_EXIT, 0);
    
    while(1);
}
