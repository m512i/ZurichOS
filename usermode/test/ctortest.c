/* Constructor Test - Tests GCC constructor/destructor attributes */

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

static int global_value = 0;
static int ctor1_ran = 0;
static int ctor2_ran = 0;

__attribute__((constructor(101)))
static void constructor1(void)
{
    print("[CTOR1] First constructor running (priority 101)\n");
    global_value = 42;
    ctor1_ran = 1;
}

__attribute__((constructor(102)))
static void constructor2(void)
{
    print("[CTOR2] Second constructor running (priority 102)\n");
    global_value += 100;
    ctor2_ran = 1;
}

__attribute__((destructor(102)))
static void destructor1(void)
{
    print("[DTOR1] First destructor running\n");
}

__attribute__((destructor(101)))
static void destructor2(void)
{
    print("[DTOR2] Second destructor running\n");
}

static void print_num(int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) {
        print("0");
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        char c[2] = {buf[--i], 0};
        print(c);
    }
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
    print("\n=== Constructor/Destructor Test ===\n\n");
    
    print("Constructors should have been called by kernel before _start.\n\n");
    
    print("Checking if constructors ran:\n");
    print("  ctor1_ran = ");
    print(ctor1_ran ? "YES" : "NO");
    print("\n");
    print("  ctor2_ran = ");
    print(ctor2_ran ? "YES" : "NO");
    print("\n");
    print("  global_value = ");
    print_num(global_value);
    print(" (expected: 142 if both ran)\n\n");
    
    if (ctor1_ran && ctor2_ran && global_value == 142) {
        print("SUCCESS: Global constructors work!\n");
    } else {
        print("PARTIAL: Constructors did not run as expected.\n");
    }
    
    print("\nExiting...\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
