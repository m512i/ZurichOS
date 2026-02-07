/* Syscall Interface
 * Core syscall handler and dispatch table
 */

#include <kernel/kernel.h>
#include <kernel/process.h>
#include <syscall/syscall.h>
#include <syscall/syscall_internal.h>
#include <arch/x86/idt.h>
#include <mm/vmm.h>

#define SYS_EXIT    0
#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_GETPID  5
#define SYS_LSEEK   6
#define SYS_STAT    7
#define SYS_FORK    8
#define SYS_EXEC    9
#define SYS_WAITPID 10
#define SYS_KILL    11
#define SYS_GETPPID 12
#define SYS_SETPGID 13
#define SYS_GETPGID 14
#define SYS_SIGACTION 15
#define SYS_SIGPROCMASK 16
#define SYS_PIPE    17
#define SYS_SHMGET  18
#define SYS_SHMAT   19
#define SYS_SHMDT   20
#define SYS_MSGGET  21
#define SYS_MSGSND  22
#define SYS_MSGRCV  23
#define SYS_MMAP    24
#define SYS_MUNMAP  25
#define SYS_MPROTECT 26
#define SYS_BRK     27
#define SYS_SOCKET  50
#define SYS_BIND    51
#define SYS_LISTEN  52
#define SYS_ACCEPT  53
#define SYS_CONNECT 54
#define SYS_SEND    55
#define SYS_RECV    56
#define SYS_CLOSESOCK 57
#define SYS_SENDTO  58
#define SYS_RECVFROM 59
#define SYS_SHUTDOWN 60
#define SYS_GETSOCKNAME 61
#define SYS_GETPEERNAME 62
#define SYS_SETSOCKOPT 63
#define SYS_GETSOCKOPT 64
#define SYS_SELECT  65
#define MAX_SYSCALL 66

fd_entry_t *get_fd_table(void)
{
    process_t *proc = process_current();
    if (proc) {
        return proc->fd_table;
    }
    return NULL;
}

int alloc_fd(void)
{
    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -1;

    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            fd_table[i].offset = 0;
            return i;
        }
    }
    return -1;
}

void free_fd(int fd)
{
    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return;

    if (fd >= 3 && fd < MAX_FDS) {
        fd_table[fd].node = NULL;
        fd_table[fd].offset = 0;
        fd_table[fd].flags = 0;
        fd_table[fd].in_use = 0;
    }
}

int validate_user_ptr(uint32_t ptr, uint32_t size)
{
    if (ptr == 0) return 0;
    if (ptr >= 0xC0000000) return 0;
    if (ptr + size < ptr) return 0;
    if (ptr + size >= 0xC0000000) return 0;

    uint32_t start_page = ptr & 0xFFFFF000;
    uint32_t end_page = (ptr + size - 1) & 0xFFFFF000;

    for (uint32_t page = start_page; page <= end_page; page += 0x1000) {
        if (!vmm_is_mapped(page)) {
            return 0;
        }
    }

    return 1;
}

int validate_user_string(uint32_t ptr, uint32_t max_len)
{
    if (ptr == 0) return 0;
    if (ptr >= 0xC0000000) return 0;

    char *str = (char *)ptr;
    for (uint32_t i = 0; i < max_len; i++) {
        uint32_t addr = ptr + i;
        if (addr >= 0xC0000000) return 0;

        if ((addr & 0xFFF) == 0) {
            if (!vmm_is_mapped(addr & 0xFFFFF000)) {
                return 0;
            }
        }

        if (str[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static syscall_handler_t syscall_table[MAX_SYSCALL] = {
    [SYS_EXIT]   = sys_exit,
    [SYS_READ]   = sys_read,
    [SYS_WRITE]  = sys_write,
    [SYS_OPEN]   = sys_open,
    [SYS_CLOSE]  = sys_close,
    [SYS_GETPID] = sys_getpid,
    [SYS_LSEEK]  = sys_lseek,
    [SYS_STAT]   = sys_stat,
    [SYS_FORK]   = sys_fork,
    [SYS_EXEC]   = sys_exec,
    [SYS_WAITPID] = sys_waitpid,
    [SYS_KILL]   = sys_kill,
    [SYS_GETPPID] = sys_getppid,
    [SYS_SETPGID] = sys_setpgid,
    [SYS_GETPGID] = sys_getpgid,
    [SYS_SIGACTION] = sys_sigaction,
    [SYS_SIGPROCMASK] = sys_sigprocmask,
    [SYS_PIPE]   = sys_pipe,
    [SYS_SHMGET] = sys_shmget,
    [SYS_SHMAT]  = sys_shmat,
    [SYS_SHMDT]  = sys_shmdt,
    [SYS_MSGGET] = sys_msgget,
    [SYS_MSGSND] = sys_msgsnd,
    [SYS_MSGRCV] = sys_msgrcv,
    [SYS_MMAP]   = sys_mmap_handler,
    [SYS_MUNMAP] = sys_munmap_handler,
    [SYS_MPROTECT] = sys_mprotect_handler,
    [SYS_BRK]    = sys_brk_handler,
    [SYS_SOCKET] = sys_socket,
    [SYS_BIND]   = sys_bind,
    [SYS_LISTEN] = sys_listen,
    [SYS_ACCEPT] = sys_accept,
    [SYS_CONNECT] = sys_connect,
    [SYS_SEND]   = sys_send,
    [SYS_RECV]   = sys_recv,
    [SYS_CLOSESOCK] = sys_closesock,
    [SYS_SHUTDOWN] = sys_shutdown,
    [SYS_GETSOCKNAME] = sys_getsockname,
    [SYS_GETPEERNAME] = sys_getpeername,
    [SYS_SETSOCKOPT] = sys_setsockopt,
    [SYS_GETSOCKOPT] = sys_getsockopt,
    [SYS_SELECT] = sys_select,
};

static void syscall_handler(registers_t *regs)
{
    uint32_t syscall_num = regs->eax;

    if (syscall_num >= MAX_SYSCALL || !syscall_table[syscall_num]) {
        regs->eax = (uint32_t)-1;
        return;
    }

    uint32_t arg0 = regs->ebx;
    uint32_t arg1 = regs->ecx;
    uint32_t arg2 = regs->edx;
    uint32_t arg3 = regs->esi;
    uint32_t arg4 = regs->edi;
    int32_t result = syscall_table[syscall_num](arg0, arg1, arg2, arg3, arg4);

    regs->eax = (uint32_t)result;
}

void syscall_init(void)
{
    register_interrupt_handler(0x80, syscall_handler);
}
