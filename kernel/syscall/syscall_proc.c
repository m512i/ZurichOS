/* Process/Signal/IPC/Memory Syscalls
 * fork, exec, wait, signals, pipes, shm, mmap
 */

#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/elf.h>
#include <kernel/signal.h>
#include <kernel/ipc.h>
#include <mm/mmap.h>
#include <syscall/syscall_internal.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <mm/heap.h>

extern void shell_run(void);

static uint32_t shell_stack_base = 0;
static uint32_t shell_stack_top = 0;

void syscall_set_shell_stack(uint32_t base, uint32_t top)
{
    shell_stack_base = base;
    shell_stack_top = top;
}

int32_t sys_exit(uint32_t status, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    serial_puts("[SYSCALL] Process exit with status ");
    char buf[12];
    int i = 0;
    uint32_t n = status;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    serial_puts(buf);
    serial_puts("\n");

    extern void vga_puts(const char *str);
    vga_puts("\n[Program exited with status ");
    vga_puts(buf);
    vga_puts("] press enter...\n");

    fd_entry_t *fdt = get_fd_table();
    if (fdt) {
        for (int fd = 3; fd < MAX_FDS; fd++) {
            if (fdt[fd].in_use) {
                free_fd(fd);
            }
        }
    }

    process_t *current = process_current();
    if (current && current->pid > 1) {
        if (current->kernel_stack) {
            kfree((void *)current->kernel_stack);
            current->kernel_stack = 0;
        }
        if (current->elf_proc) {
            elf_free_process((user_process_t *)current->elf_proc);
            current->elf_proc = NULL;
        }
        process_kill(current->pid);
    }

    process_set_current(1);

    extern void *kmalloc(uint32_t size);
    uint32_t new_stack;
    if (shell_stack_top != 0) {
        new_stack = shell_stack_top;
    } else {
        new_stack = (uint32_t)kmalloc(8192) + 8192;
    }

    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "mov %0, %%esp\n"
        "mov %0, %%ebp\n"
        "sti\n"
        "jmp *%1\n"
        :
        : "r"(new_stack), "r"(shell_run)
        : "eax", "memory"
    );

    cli();
    for (;;) {
        hlt();
    }

    return 0;
}

int32_t sys_getpid(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg0; (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    process_t *proc = process_current();
    if (proc) {
        return (int32_t)proc->pid;
    }
    return 1;
}

int32_t sys_fork(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg0; (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return process_fork();
}

int32_t sys_exec(uint32_t path, uint32_t argv, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    if (!validate_user_string(path, 256)) return -14;
    return process_exec((const char *)path, (char *const *)argv);
}

int32_t sys_waitpid(uint32_t pid, uint32_t status, uint32_t options, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    int32_t *status_ptr = NULL;
    if (status && validate_user_ptr(status, sizeof(int32_t))) {
        status_ptr = (int32_t *)status;
    }
    return process_waitpid((int32_t)pid, status_ptr, (int)options);
}

int32_t sys_kill(uint32_t pid, uint32_t sig, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    return process_signal(pid, (int)sig);
}

int32_t sys_getppid(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg0; (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    process_t *proc = process_current();
    if (proc) {
        return (int32_t)proc->ppid;
    }
    return 0;
}

int32_t sys_setpgid(uint32_t pid, uint32_t pgid, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    return process_setpgid(pid, pgid);
}

int32_t sys_getpgid(uint32_t pid, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return process_getpgid(pid);
}

int32_t sys_sigaction(uint32_t sig, uint32_t handler, uint32_t oldact, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    sigaction_t *old = NULL;
    if (oldact && validate_user_ptr(oldact, sizeof(sigaction_t))) {
        old = (sigaction_t *)oldact;
    }
    return signal_set_handler((int)sig, (sighandler_t)handler, old);
}

int32_t sys_sigprocmask(uint32_t how, uint32_t set, uint32_t oldset, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    sigset_t *s = NULL, *os = NULL;
    if (set && validate_user_ptr(set, sizeof(sigset_t))) s = (sigset_t *)set;
    if (oldset && validate_user_ptr(oldset, sizeof(sigset_t))) os = (sigset_t *)oldset;
    
    if (how == 0) return signal_block(s, os);
    else if (how == 1) return signal_unblock(s, os);
    else return signal_setmask(s, os);
}

int32_t sys_pipe(uint32_t pipefd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    if (!validate_user_ptr(pipefd, 2 * sizeof(int))) return -14;
    return pipe_create((int *)pipefd);
}

int32_t sys_shmget(uint32_t key, uint32_t size, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    return shm_create(key, size);
}

int32_t sys_shmat(uint32_t shmid, uint32_t vaddr, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    void *result = shm_attach((int)shmid, vaddr);
    return (int32_t)(uint32_t)result;
}

int32_t sys_shmdt(uint32_t addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return shm_detach((void *)addr);
}

int32_t sys_msgget(uint32_t key, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return msgq_create(key);
}

int32_t sys_msgsnd(uint32_t msqid, uint32_t msgp, uint32_t msgsz, uint32_t mtype, uint32_t arg4)
{
    (void)arg4;
    if (!validate_user_ptr(msgp, msgsz)) return -14;
    return msgq_send((int)msqid, (const void *)msgp, msgsz, (long)mtype);
}

int32_t sys_msgrcv(uint32_t msqid, uint32_t msgp, uint32_t msgsz, uint32_t mtype, uint32_t arg4)
{
    (void)arg4;
    if (!validate_user_ptr(msgp, msgsz)) return -14;
    return msgq_receive((int)msqid, (void *)msgp, msgsz, (long)mtype);
}

int32_t sys_mmap_handler(uint32_t addr, uint32_t length, uint32_t prot, uint32_t flags, uint32_t fd)
{
    void *result = sys_mmap((void *)addr, length, (int)prot, (int)flags, (int)fd, 0);
    return (int32_t)(uint32_t)result;
}

int32_t sys_munmap_handler(uint32_t addr, uint32_t length, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    return sys_munmap((void *)addr, length);
}

int32_t sys_mprotect_handler(uint32_t addr, uint32_t length, uint32_t prot, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    return sys_mprotect((void *)addr, length, (int)prot);
}

int32_t sys_brk_handler(uint32_t addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return (int32_t)addr;
}
