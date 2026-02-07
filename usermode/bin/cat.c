/* Cat Utility
 * Display file contents
 */

#define SYS_EXIT    0
#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_OPEN    3
#define SYS_CLOSE   4

static inline int syscall0(int num)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

static void print(const char *str)
{
    int len = 0;
    while (str[len]) len++;
    syscall3(SYS_WRITE, 1, (int)str, len);
}

void _start(void)
{
    /* For now, just print a message since we don't have command line args */
    print("cat: Usage: cat <filename>\n");
    print("      (Command line arguments not yet implemented)\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
