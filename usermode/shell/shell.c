/* User-space Shell
 * Interactive command interpreter running in Ring 3
 */

#define NULL ((void *)0)

#define SYS_EXIT    0
#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_FORK    8
#define SYS_EXEC    9
#define SYS_WAITPID 10

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

static int read_line(char *buf, int max_len)
{
    int pos = 0;
    char c;
    
    while (pos < max_len - 1) {
        int bytes = syscall3(SYS_READ, 0, (int)&c, 1);
        if (bytes <= 0) break;
        
        if (c == '\n' || c == '\r') {
            buf[pos] = '\0';
            syscall1(SYS_WRITE, '\n');
            return pos;
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                syscall1(SYS_WRITE, '\b');
                syscall1(SYS_WRITE, ' ');
                syscall1(SYS_WRITE, '\b');
            }
        } else if (c >= 32 && c < 127) {
            buf[pos++] = c;
            syscall1(SYS_WRITE, c);
        }
    }
    
    buf[pos] = '\0';
    return pos;
}

static int execute_command(const char *cmd)
{
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    
    if (*cmd == '\0') return 0;
    
    if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't') {
        print("Goodbye!\n");
        syscall1(SYS_EXIT, 0);
        return 0;
    }
    
    int pid = syscall0(SYS_FORK);
    if (pid == 0) {
        const char *argv[2];
        argv[0] = cmd;
        argv[1] = NULL;
        
        char path[256];
        int i = 0;
        while (cmd[i] && cmd[i] != ' ' && i < 250) {
            path[i] = cmd[i];
            i++;
        }
        path[i] = '\0';
        
        syscall3(SYS_EXEC, (int)path, (int)argv, 0);
        
        print("Command not found: ");
        print(cmd);
        print("\n");
        syscall1(SYS_EXIT, 1);
    } else if (pid > 0) {
        int status;
        syscall3(SYS_WAITPID, pid, (int)&status, 0);
        return status;
    } else {
        print("Fork failed\n");
        return -1;
    }
}

void _start(void)
{
    print("=== ZurichOS User Shell ===\n");
    print("Type 'exit' to quit\n\n");
    
    char cmd[256];
    
    while (1) {
        print("$ ");
        read_line(cmd, sizeof(cmd));
        execute_command(cmd);
    }
}
