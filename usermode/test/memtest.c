/* Memory test program - tests user memory access */

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
    char buf[9];
    for (int i = 7; i >= 0; i--) {
        int nibble = (n >> (i * 4)) & 0xF;
        buf[7-i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buf[8] = '\0';
    print(buf);
}

void _start(void)
{
    print("Memory Test Program\n");
    print("===================\n\n");
    
    print("Testing stack memory...\n");
    volatile int stack_var = 0x12345678;
    print("  Stack variable at 0x");
    print_hex((unsigned int)&stack_var);
    print(" = 0x");
    print_hex(stack_var);
    print("\n");
    
    print("  Writing 0xDEADBEEF...\n");
    stack_var = 0xDEADBEEF;
    print("  Read back: 0x");
    print_hex(stack_var);
    if (stack_var == 0xDEADBEEF) {
        print(" [OK]\n");
    } else {
        print(" [FAIL]\n");
    }
    
    print("\nTesting array on stack...\n");
    int arr[5] = {1, 2, 3, 4, 5};
    int sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += arr[i];
    }
    print("  Sum of [1,2,3,4,5] = ");
    char buf[12];
    int idx = 0;
    int n = sum;
    while (n > 0) {
        buf[idx++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = 0; j < idx / 2; j++) {
        char t = buf[j];
        buf[j] = buf[idx - 1 - j];
        buf[idx - 1 - j] = t;
    }
    buf[idx] = '\0';
    print(buf);
    if (sum == 15) {
        print(" [OK]\n");
    } else {
        print(" [FAIL]\n");
    }
    
    print("\nAll tests passed!\n");
    syscall1(SYS_EXIT, 0);
    while(1);
}
