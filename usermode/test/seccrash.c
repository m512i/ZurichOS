/* Security Crash Test Program
 * This program intentionally triggers security violations to verify protections work.
 * Run with argument to select test:
 *   1 = Stack buffer overflow (tests stack canary)
 *   2 = Kernel memory read (tests memory protection)
 *   3 = Null pointer dereference (tests null page protection)
 *   4 = Execute data as code (tests NX if available)
 */

#define NULL ((void *)0)

#define SYS_EXIT    0
#define SYS_WRITE   2

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

/* Test 1: Stack buffer overflow - should trigger stack canary */
static void __attribute__((noinline)) test_stack_overflow(void)
{
    print("[TEST] Stack buffer overflow attack...\n");
    print("       If stack canary works, you'll see a crash message.\n");
    print("       Smashing stack now...\n\n");
    
    char buffer[16];
    /* Intentionally overflow the buffer */
    volatile char *p = buffer;
    for (int i = 0; i < 128; i++) {
        p[i] = 'A';
    }
    
    print("[FAIL] Stack overflow completed without detection!\n");
    print("       Stack canary protection FAILED!\n");
}

/* Test 2: Kernel memory read - should trigger page fault */
static void test_kernel_read(void)
{
    print("[TEST] Attempting to read kernel memory at 0xC0100000...\n");
    print("       If memory protection works, you'll see a page fault.\n\n");
    
    volatile unsigned int *kernel_ptr = (volatile unsigned int *)0xC0100000;
    unsigned int value = *kernel_ptr;
    
    print("[FAIL] Read kernel memory successfully! Value: ");
    /* Print hex */
    char hex[11] = "0x00000000";
    const char *digits = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        hex[i] = digits[value & 0xF];
        value >>= 4;
    }
    print(hex);
    print("\n       Memory protection FAILED!\n");
}

/* Test 3: Null pointer dereference - should trigger page fault */
static void test_null_deref(void)
{
    print("[TEST] Attempting null pointer dereference...\n");
    print("       If null page protection works, you'll see a page fault.\n\n");
    
    volatile unsigned int *null_ptr = (volatile unsigned int *)0x00000000;
    unsigned int value = *null_ptr;
    (void)value;
    
    print("[FAIL] Null dereference succeeded!\n");
    print("       Null page protection FAILED!\n");
}

/* Test 4: Execute data as code - should trigger fault if NX enabled */
static void test_execute_data(void)
{
    print("[TEST] Attempting to execute data as code...\n");
    print("       If NX bit is enabled, you'll see a fault.\n");
    print("       (Note: NX requires PAE mode and CPU support)\n\n");
    
    /* Machine code for: mov eax, 0x12345678; ret */
    unsigned char code[] = {0xB8, 0x78, 0x56, 0x34, 0x12, 0xC3};
    
    /* Try to execute it */
    typedef int (*func_t)(void);
    func_t func = (func_t)code;
    int result = func();
    
    print("[WARN] Executed data as code! Result: ");
    char buf[12];
    int i = 0;
    unsigned int n = (unsigned int)result;
    if (n == 0) buf[i++] = '0';
    else {
        while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    print(buf);
    print("\n       NX protection not active (expected without PAE)\n");
}

void _start(void)
{
    print("=== ZurichOS Security Crash Test ===\n");
    print("This program intentionally triggers security violations.\n");
    print("A crash/fault means the protection is WORKING.\n\n");
    
    print("Select test:\n");
    print("  1 = Stack overflow (stack canary test)\n");
    print("  2 = Kernel memory read (memory protection test)\n");
    print("  3 = Null pointer dereference\n");
    print("  4 = Execute data as code (NX test)\n");
    print("  0 = Run all tests sequentially\n\n");
    
    /* For now, run test 2 (kernel read) as it's the most reliable */
    print("Running Test 2: Kernel Memory Read...\n\n");
    test_kernel_read();
    
    print("\n=== If you see this, the test FAILED ===\n");
    print("The security protection did not trigger.\n");
    
    syscall1(SYS_EXIT, 1);
    while(1);
}
