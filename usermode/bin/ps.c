/* Process Status Utility
 * List running processes
 */

#define SYS_EXIT    0
#define SYS_WRITE   2
#define SYS_GETPID  5

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

static void print_num(int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    print(buf);
}

void _start(void)
{
    print("PID  NAME\n");
    print("---  -----\n");
    
    int my_pid = syscall0(SYS_GETPID);
    print_num(my_pid);
    print("  ps\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
