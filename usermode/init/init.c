/* Init Process - PID 1
 * First user-space process, parent of all other processes
 */

#define NULL ((void *)0)

#define SYS_EXIT    0
#define SYS_FORK    8
#define SYS_EXEC    9
#define SYS_WAITPID 10
#define SYS_OPEN    3
#define SYS_WRITE   2

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

void _start(void)
{
    print("=== ZurichOS Init Process (PID 1) ===\n");
    
    print("[init] Mounting filesystems...\n");
    
    print("[init] Starting system services...\n");
    
    int shell_pid = syscall0(SYS_FORK);
    if (shell_pid == 0) {
        const char *shell_path = "/shell.elf";
        const char *argv[] = {shell_path, NULL};
        syscall3(SYS_EXEC, (int)shell_path, (int)argv, 0);
        print("[init] Failed to start shell!\n");
        syscall1(SYS_EXIT, 1);
    } else if (shell_pid > 0) {
        print("[init] Started shell with PID ");
        char buf[12];
        int i = 0;
        int n = shell_pid;
        if (n == 0) buf[i++] = '0';
        else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
        buf[i] = '\0';
        for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
        print(buf);
        print("\n");
        
        int status;
        syscall3(SYS_WAITPID, shell_pid, (int)&status, 0);
        
        print("[init] Shell exited, rebooting system...\n");
    } else {
        print("[init] Failed to fork for shell!\n");
    }
    
    syscall1(SYS_EXIT, 1);
    while(1);
}
