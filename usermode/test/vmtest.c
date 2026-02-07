/* Virtual Memory Test Program
 * Tests mmap, munmap, mprotect, demand paging
 */

#define NULL ((void *)0)

#define SYS_EXIT    0
#define SYS_WRITE   2
#define SYS_MMAP    24
#define SYS_MUNMAP  25
#define SYS_MPROTECT 26

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20

#define MAP_FAILED  ((void *)-1)

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2) : "memory");
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

static inline int syscall5(int num, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
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
    char hex[11] = "0x";
    for (int k = 7; k >= 0; k--) {
        int nibble = (n >> (k * 4)) & 0xF;
        hex[9-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[10] = '\0';
    print(hex);
}

static void print_num(int n)
{
    char buf[12];
    int i = 0;
    int neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) buf[i++] = '0';
    else while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    buf[i] = '\0';
    print(buf);
}

static void *mmap(void *addr, unsigned int len, int prot, int flags, int fd)
{
    return (void *)syscall5(SYS_MMAP, (int)addr, len, prot, flags, fd);
}

static int munmap(void *addr, unsigned int len)
{
    return syscall2(SYS_MUNMAP, (int)addr, len);
}

static int mprotect(void *addr, unsigned int len, int prot)
{
    return syscall3(SYS_MPROTECT, (int)addr, len, prot);
}

void _start(void)
{
    print("=== Virtual Memory Test Program ===\n\n");
    
    /* Test 1: Anonymous mmap */
    print("Test 1: Anonymous mmap (4KB)\n");
    void *ptr1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1);
    print("  mmap returned: ");
    print_hex((unsigned int)ptr1);
    print("\n");
    
    if (ptr1 != MAP_FAILED) {
        /* Write to the mapped memory */
        print("  Writing to mapped memory...\n");
        char *p = (char *)ptr1;
        p[0] = 'H';
        p[1] = 'e';
        p[2] = 'l';
        p[3] = 'l';
        p[4] = 'o';
        p[5] = '\0';
        print("  Read back: ");
        print(p);
        print("\n");
    }
    print("\n");
    
    /* Test 2: Larger anonymous mmap */
    print("Test 2: Anonymous mmap (64KB)\n");
    void *ptr2 = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1);
    print("  mmap returned: ");
    print_hex((unsigned int)ptr2);
    print("\n");
    
    if (ptr2 != MAP_FAILED) {
        /* Write pattern to memory */
        print("  Writing pattern to 64KB region...\n");
        unsigned int *arr = (unsigned int *)ptr2;
        for (int i = 0; i < 16; i++) {
            arr[i] = i * 0x1000;
        }
        print("  First 4 values: ");
        for (int i = 0; i < 4; i++) {
            print_hex(arr[i]);
            print(" ");
        }
        print("\n");
    }
    print("\n");
    
    /* Test 3: mprotect - change to read-only */
    print("Test 3: mprotect (change to read-only)\n");
    if (ptr1 != MAP_FAILED) {
        int result = mprotect(ptr1, 4096, PROT_READ);
        print("  mprotect returned: ");
        print_num(result);
        print("\n");
        print("  Memory is now read-only\n");
    }
    print("\n");
    
    /* Test 4: munmap */
    print("Test 4: munmap\n");
    if (ptr1 != MAP_FAILED) {
        int result = munmap(ptr1, 4096);
        print("  munmap(ptr1) returned: ");
        print_num(result);
        print("\n");
    }
    if (ptr2 != MAP_FAILED) {
        int result = munmap(ptr2, 65536);
        print("  munmap(ptr2) returned: ");
        print_num(result);
        print("\n");
    }
    print("\n");
    
    /* Test 5: Fixed address mmap */
    print("Test 5: Fixed address mmap\n");
    void *fixed_addr = (void *)0x50000000;
    void *ptr3 = mmap(fixed_addr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | 0x10, -1);
    print("  Requested: ");
    print_hex((unsigned int)fixed_addr);
    print("\n  Got: ");
    print_hex((unsigned int)ptr3);
    print("\n");
    if (ptr3 != MAP_FAILED) {
        munmap(ptr3, 4096);
    }
    print("\n");
    
    print("=== All VM tests completed! ===\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
