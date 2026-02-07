/* IPC Test Program
 * Tests pipes, signals, fork, wait, shared memory, message queues
 */

#define SYS_EXIT    0
#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_GETPID  5
#define SYS_FORK    8
#define SYS_WAITPID 10
#define SYS_KILL    11
#define SYS_GETPPID 12
#define SYS_PIPE    17
#define SYS_SHMGET  18
#define SYS_SHMAT   19
#define SYS_SHMDT   20
#define SYS_MSGGET  21
#define SYS_MSGSND  22
#define SYS_MSGRCV  23

#define SIGTERM 15

static inline int syscall0(int num)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
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

static inline int syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2)
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

static inline int syscall4(int num, int arg1, int arg2, int arg3, int arg4)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
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
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) buf[i++] = '0';
    else while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t;
    }
    buf[i] = '\0';
    print(buf);
}

static int getpid(void) { return syscall0(SYS_GETPID); }
static int getppid(void) { return syscall0(SYS_GETPPID); }
static int fork_syscall(void) { return syscall0(SYS_FORK); }
static int waitpid(int pid, int *status, int options) { 
    return syscall3(SYS_WAITPID, pid, (int)status, options); 
}
static int kill_syscall(int pid, int sig) { return syscall2(SYS_KILL, pid, sig); }
static int pipe_syscall(int *pipefd) { return syscall1(SYS_PIPE, (int)pipefd); }
static int shmget(int key, int size) { return syscall2(SYS_SHMGET, key, size); }
static int shmat(int shmid, int vaddr) { return syscall2(SYS_SHMAT, shmid, vaddr); }
static int shmdt(int addr) { return syscall1(SYS_SHMDT, addr); }
static int msgget(int key) { return syscall1(SYS_MSGGET, key); }
static int msgsnd(int msqid, const void *msg, int size, int mtype) {
    return syscall4(SYS_MSGSND, msqid, (int)msg, size, mtype);
}
static int msgrcv(int msqid, void *msg, int size, int mtype) {
    return syscall4(SYS_MSGRCV, msqid, (int)msg, size, mtype);
}

void _start(void)
{
    print("=== IPC Test Program ===\n\n");
    
    /* Test 1: getpid/getppid */
    print("Test 1: Process IDs\n");
    print("  My PID: ");
    print_num(getpid());
    print("\n  Parent PID: ");
    print_num(getppid());
    print("\n\n");
    
    /* Test 2: Fork (simplified - just tests syscall) */
    print("Test 2: Fork syscall\n");
    int fork_result = fork_syscall();
    print("  fork() returned: ");
    print_num(fork_result);
    print("\n");
    if (fork_result > 0) {
        print("  Created child with PID ");
        print_num(fork_result);
        print("\n\n");
    } else if (fork_result == 0) {
        print("  I am the child!\n\n");
    } else {
        print("  Fork failed\n\n");
    }
    
    /* Test 3: Pipe */
    print("Test 3: Pipe creation\n");
    int pipefd[2] = {-1, -1};
    int pipe_result = pipe_syscall(pipefd);
    print("  pipe() returned: ");
    print_num(pipe_result);
    print("\n");
    if (pipe_result == 0) {
        print("  Read fd: ");
        print_num(pipefd[0]);
        print(", Write fd: ");
        print_num(pipefd[1]);
        print("\n");
    }
    print("\n");
    
    /* Test 4: Shared Memory */
    print("Test 4: Shared Memory\n");
    int shmid = shmget(12345, 4096);
    print("  shmget() returned: ");
    print_num(shmid);
    print("\n");
    if (shmid >= 0) {
        int addr = shmat(shmid, 0x40000000);
        print("  shmat() returned: 0x");
        /* Print hex */
        char hex[9];
        for (int k = 7; k >= 0; k--) {
            int nibble = (addr >> (k * 4)) & 0xF;
            hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        }
        hex[8] = '\0';
        print(hex);
        print("\n");
        shmdt(addr);
        print("  Detached shared memory\n");
    }
    print("\n");
    
    /* Test 5: Message Queue */
    print("Test 5: Message Queue\n");
    int msqid = msgget(54321);
    print("  msgget() returned: ");
    print_num(msqid);
    print("\n");
    if (msqid >= 0) {
        const char *msg = "Hello IPC!";
        int send_result = msgsnd(msqid, msg, 10, 1);
        print("  msgsnd() returned: ");
        print_num(send_result);
        print("\n");
        
        char recv_buf[32] = {0};
        int recv_result = msgrcv(msqid, recv_buf, 32, 1);
        print("  msgrcv() returned: ");
        print_num(recv_result);
        print("\n");
        if (recv_result > 0) {
            print("  Received: ");
            print(recv_buf);
            print("\n");
        }
    }
    print("\n");
    
    /* Test 6: Kill (send signal to self) */
    print("Test 6: Signal (kill)\n");
    print("  Sending signal 0 to self (existence check)...\n");
    int kill_result = kill_syscall(getpid(), 0);
    print("  kill() returned: ");
    print_num(kill_result);
    print(" (0 = success)\n\n");
    
    print("=== All IPC tests completed! ===\n");
    
    syscall1(SYS_EXIT, 0);
    while(1);
}
