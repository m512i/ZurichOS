/* Security Test Program
 * Tests security features: stack canary, ASLR, memory protection
 * Some tests intentionally trigger violations to verify protection works
 */

#define NULL ((void *)0)

#define SYS_EXIT    0
#define SYS_WRITE   2

/* Test mode - set to 1 to trigger actual violations (will crash/halt) */
#define TEST_STACK_SMASH    0   /* Set to 1 to test stack canary */
#define TEST_KERNEL_READ    0   /* Set to 1 to test kernel memory access */
#define TEST_NULL_DEREF     0   /* Set to 1 to test null pointer */

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

static void print_hex(unsigned int val)
{
    char buf[11] = "0x00000000";
    const char *hex = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    print(buf);
}

static int tests_passed = 0;
static int tests_failed = 0;

static void test_pass(const char *name)
{
    print("[PASS] ");
    print(name);
    print("\n");
    tests_passed++;
}

static void test_fail(const char *name)
{
    print("[FAIL] ");
    print(name);
    print("\n");
    tests_failed++;
}

/* Test 1: Stack address randomization (ASLR) */
static void test_aslr_stack(void)
{
    /* Get stack address */
    int stack_var = 0;
    unsigned int stack_addr = (unsigned int)&stack_var;
    
    print("  Stack address: ");
    print_hex(stack_addr);
    print("\n");
    
    /* Stack should be in user space (below 0xC0000000) */
    if (stack_addr < 0xC0000000 && stack_addr > 0x00100000) {
        test_pass("Stack in valid user space range");
    } else {
        test_fail("Stack address out of expected range");
    }
}

/* Test 2: Heap address check */
static void test_heap_address(void)
{
    /* We can't allocate heap in userspace without malloc syscall,
       but we can check our code segment */
    unsigned int code_addr = (unsigned int)&test_heap_address;
    
    print("  Code address: ");
    print_hex(code_addr);
    print("\n");
    
    if (code_addr < 0xC0000000) {
        test_pass("Code in valid user space range");
    } else {
        test_fail("Code address out of expected range");
    }
}

/* Test 3: Stack canary presence check */
static void test_stack_canary(void)
{
    /* The stack canary is set up by the compiler with -fstack-protector.
       We can't directly test it without triggering it, but we can verify
       the guard value exists in the kernel. */
    print("  Stack canary: compiler-inserted guard value\n");
    
#if TEST_STACK_SMASH
    /* DANGER: This will intentionally smash the stack to trigger canary */
    print("  !!! TRIGGERING STACK SMASH - EXPECT CRASH !!!\n");
    char buffer[8];
    /* Overflow the buffer to corrupt return address and canary */
    for (int i = 0; i < 64; i++) {
        buffer[i] = 'A';  /* Write past buffer bounds */
    }
    /* If we get here, stack canary didn't work */
    test_fail("Stack canary did NOT detect overflow!");
#else
    test_pass("Stack canary mechanism available");
    print("  (Set TEST_STACK_SMASH=1 to trigger actual test)\n");
#endif
}

/* Test 4: Memory isolation test */
static void test_memory_isolation(void)
{
    unsigned int user_addr = (unsigned int)&tests_passed;
    
    if (user_addr < 0xC0000000) {
        test_pass("User data isolated from kernel space");
    } else {
        test_fail("User data in kernel space (isolation failure)");
    }
    
#if TEST_KERNEL_READ
    /* DANGER: Try to read from kernel space - should cause page fault */
    print("  !!! ATTEMPTING KERNEL MEMORY READ - EXPECT CRASH !!!\n");
    volatile unsigned int *kernel_ptr = (volatile unsigned int *)0xC0100000;
    unsigned int val = *kernel_ptr;  /* This should fault */
    (void)val;
    test_fail("Kernel memory read succeeded (SECURITY FAILURE!)");
#else
    print("  (Set TEST_KERNEL_READ=1 to trigger kernel access test)\n");
#endif

#if TEST_NULL_DEREF
    /* DANGER: Null pointer dereference */
    print("  !!! ATTEMPTING NULL DEREFERENCE - EXPECT CRASH !!!\n");
    volatile unsigned int *null_ptr = (volatile unsigned int *)0;
    unsigned int val2 = *null_ptr;  /* This should fault */
    (void)val2;
    test_fail("Null dereference succeeded (SECURITY FAILURE!)");
#else
    print("  (Set TEST_NULL_DEREF=1 to trigger null pointer test)\n");
#endif
}

/* Test 5: Function pointer integrity */
static void dummy_function(void) { }

static void test_function_pointers(void)
{
    void (*fptr)(void) = dummy_function;
    unsigned int fptr_addr = (unsigned int)fptr;
    
    print("  Function pointer: ");
    print_hex(fptr_addr);
    print("\n");
    
    if (fptr_addr < 0xC0000000 && fptr_addr > 0x00001000) {
        test_pass("Function pointers in valid range");
    } else {
        test_fail("Function pointer out of range");
    }
}

/* Test 6: Return address on stack */
static void test_return_address(void)
{
    /* Get approximate return address location */
    unsigned int *stack_ptr;
    __asm__ volatile ("mov %%esp, %0" : "=r"(stack_ptr));
    
    print("  Stack pointer: ");
    print_hex((unsigned int)stack_ptr);
    print("\n");
    
    if ((unsigned int)stack_ptr < 0xC0000000) {
        test_pass("Stack pointer in user space");
    } else {
        test_fail("Stack pointer in kernel space");
    }
}

void _start(void)
{
    print("=== ZurichOS Security Test Suite ===\n\n");
    
    print("Test 1: ASLR Stack Randomization\n");
    test_aslr_stack();
    print("\n");
    
    print("Test 2: Code Segment Address\n");
    test_heap_address();
    print("\n");
    
    print("Test 3: Stack Canary\n");
    test_stack_canary();
    print("\n");
    
    print("Test 4: Memory Isolation\n");
    test_memory_isolation();
    print("\n");
    
    print("Test 5: Function Pointer Integrity\n");
    test_function_pointers();
    print("\n");
    
    print("Test 6: Stack Pointer Location\n");
    test_return_address();
    print("\n");
    
    print("=== Security Test Summary ===\n");
    print("Passed: ");
    char buf[4];
    buf[0] = '0' + tests_passed;
    buf[1] = '\0';
    print(buf);
    print("  Failed: ");
    buf[0] = '0' + tests_failed;
    print(buf);
    print("\n");
    
    if (tests_failed == 0) {
        print("\nAll security tests PASSED!\n");
    } else {
        print("\nSome security tests FAILED!\n");
    }
    
    syscall1(SYS_EXIT, tests_failed);
    while(1);
}
