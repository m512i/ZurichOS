/* File I/O Test - Tests sys_open, sys_read, sys_write, sys_close, sys_lseek */

#define SYS_EXIT   0
#define SYS_READ   1
#define SYS_WRITE  2
#define SYS_OPEN   3
#define SYS_CLOSE  4
#define SYS_LSEEK  6

#define O_RDONLY   0x0001
#define O_WRONLY   0x0002
#define O_RDWR     0x0003
#define O_CREAT    0x0100
#define O_TRUNC    0x0200
#define O_APPEND   0x0008

#define SEEK_SET   0
#define SEEK_CUR   1
#define SEEK_END   2

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
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    if (neg) buf[i++] = '-';
    
    char rev[12];
    for (int j = 0; j < i; j++) {
        rev[j] = buf[i - 1 - j];
    }
    rev[i] = '\0';
    print(rev);
}

static int open(const char *path, int flags)
{
    return syscall3(SYS_OPEN, (int)path, flags, 0);
}

static int close(int fd)
{
    return syscall1(SYS_CLOSE, fd);
}

static int read(int fd, char *buf, int count)
{
    return syscall3(SYS_READ, fd, (int)buf, count);
}

static int write(int fd, const char *buf, int count)
{
    return syscall3(SYS_WRITE, fd, (int)buf, count);
}

static int lseek(int fd, int offset, int whence)
{
    return syscall3(SYS_LSEEK, fd, offset, whence);
}

void _start(void)
{
    print("=== File I/O Syscall Test ===\n\n");
    
    print("Test 1: Create and write to file\n");
    int fd = open("/test.txt", O_WRONLY | O_CREAT | O_TRUNC);
    print("  open() returned: ");
    print_num(fd);
    print("\n");
    
    if (fd >= 0) {
        const char *msg = "Hello from userspace!\n";
        int len = 0;
        while (msg[len]) len++;
        
        int written = write(fd, msg, len);
        print("  write() returned: ");
        print_num(written);
        print("\n");
        
        int ret = close(fd);
        print("  close() returned: ");
        print_num(ret);
        print("\n");
    }
    
    print("\nTest 2: Read file back\n");
    fd = open("/test.txt", O_RDONLY);
    print("  open() returned: ");
    print_num(fd);
    print("\n");
    
    if (fd >= 0) {
        char buf[64];
        for (int i = 0; i < 64; i++) buf[i] = 0;
        
        int bytes = read(fd, buf, 63);
        print("  read() returned: ");
        print_num(bytes);
        print("\n");
        print("  Content: ");
        print(buf);
        
        close(fd);
    }
    
    print("\nTest 3: lseek test\n");
    fd = open("/test.txt", O_RDONLY);
    if (fd >= 0) {
        int pos = lseek(fd, 6, SEEK_SET);
        print("  lseek(6, SEEK_SET) returned: ");
        print_num(pos);
        print("\n");
        
        char buf[32];
        for (int i = 0; i < 32; i++) buf[i] = 0;
        int bytes = read(fd, buf, 10);
        print("  read 10 bytes: '");
        print(buf);
        print("'\n");
        
        close(fd);
    }
    
    print("\nTest 4: Open non-existent file (should fail)\n");
    fd = open("/nonexistent.txt", O_RDONLY);
    print("  open() returned: ");
    print_num(fd);
    print(" (expected: -2 ENOENT)\n");
    
    print("\n=== All tests complete ===\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
