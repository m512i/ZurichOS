/* Memory Isolation Test - Verifies user can't access kernel memory */

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

static void print_hex(unsigned int n)
{
    const char *hex = "0123456789ABCDEF";
    char buf[9];
    for (int i = 7; i >= 0; i--) {
        buf[7 - i] = hex[(n >> (i * 4)) & 0xF];
    }
    buf[8] = 0;
    print("0x");
    print(buf);
}

void _start(void)
{
    print("=== Memory Isolation Test ===\n\n");
    print("This test attempts to access kernel memory from user-space.\n");
    print("If isolation works, this should cause a page fault.\n\n");
    
    print("User-space addresses (should work):\n");
    print("  Our code is at: ");
    print_hex((unsigned int)_start);
    print("\n");
    
    volatile unsigned int *user_ptr = (volatile unsigned int *)0x08048000;
    print("  Reading 0x08048000: ");
    print_hex(*user_ptr);
    print(" - OK\n\n");
    
    print("Kernel addresses (should FAIL with page fault):\n");
    print("  Attempting to read kernel memory at 0xC0100000...\n");
    print("  If you see this, isolation may have failed!\n\n");
    
    volatile unsigned int *kernel_ptr = (volatile unsigned int *)0xC0100000;
    unsigned int val = *kernel_ptr;
    
    print("  WARNING: Read succeeded! Value = ");
    print_hex(val);
    print("\n");
    print("  ISOLATION FAILED - User can read kernel memory!\n");
    
    syscall1(SYS_EXIT, 1);
    while(1);
}
