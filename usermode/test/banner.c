/* Banner program - displays ASCII art banner */

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

void _start(void)
{
    print("\n");
    print("  ______          _      _      ____   _____ \n");
    print(" |___  /         (_)    | |    / __ \\ / ____|\n");
    print("    / /_   _ _ __ _  ___| |__ | |  | | (___  \n");
    print("   / /| | | | '__| |/ __| '_ \\| |  | |\\___ \\ \n");
    print("  / /_| |_| | |  | | (__| | | | |__| |____) |\n");
    print(" /_____\\__,_|_|  |_|\\___|_| |_|\\____/|_____/ \n");
    print("\n");
    print("  Welcome to ZurichOS - A hobby operating system\n");
    print("  Running in Ring 3 user mode!\n");
    print("\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
