/* Process Management
 * Full process control with fork/exec/wait/signals
 */

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/elf.h>
#include <kernel/kernel.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <string.h>

static process_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t current_pid = 0;

static const char *state_names[] = {
    "unused",
    "running",
    "ready",
    "blocked",
    "zombie",
    "stopped"
};

static void init_process_fd_table(process_t *proc)
{
    for (int i = 0; i < MAX_FDS_PER_PROC; i++) {
        proc->fd_table[i].node = NULL;
        proc->fd_table[i].offset = 0;
        proc->fd_table[i].flags = 0;
        proc->fd_table[i].in_use = 0;
        proc->fd_table[i].pipe_id = -1;
    }
    proc->fd_table[0].in_use = 1;
    proc->fd_table[1].in_use = 1;
    proc->fd_table[2].in_use = 1;
}

static void init_process_signals(process_t *proc)
{
    proc->pending_signals = 0;
    proc->blocked_signals = 0;
    for (int i = 0; i < NSIG; i++) {
        proc->signal_handlers[i] = (sighandler_t)0;  
    }
}

void process_init(void)
{
    memset(process_table, 0, sizeof(process_table));
    
    process_table[0].pid = 0;
    process_table[0].ppid = 0;
    process_table[0].pgid = 0;
    process_table[0].state = PROC_STATE_RUNNING;
    strcpy(process_table[0].name, "kernel");
    process_table[0].start_time = 0;
    process_table[0].cpu_time = 0;
    process_table[0].priority = 0;
    init_process_fd_table(&process_table[0]);
    init_process_signals(&process_table[0]);
    
    process_table[1].pid = 1;
    process_table[1].ppid = 0;
    process_table[1].pgid = 1;
    process_table[1].state = PROC_STATE_RUNNING;
    strcpy(process_table[1].name, "shell");
    process_table[1].start_time = 0;
    process_table[1].cpu_time = 0;
    process_table[1].priority = 1;
    init_process_fd_table(&process_table[1]);
    init_process_signals(&process_table[1]);
    
    next_pid = 2;
    current_pid = 1;
}

process_t *process_get(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_UNUSED && 
            process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

uint32_t process_count(void)
{
    uint32_t count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_UNUSED) {
            count++;
        }
    }
    return count;
}

process_t *process_iterate(uint32_t *index)
{
    while (*index < MAX_PROCESSES) {
        if (process_table[*index].state != PROC_STATE_UNUSED) {
            return &process_table[(*index)++];
        }
        (*index)++;
    }
    return NULL;
}

uint32_t process_create(const char *name, uint32_t ppid)
{
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_STATE_UNUSED) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return 0;
    }
    
    process_table[slot].pid = next_pid++;
    process_table[slot].ppid = ppid;
    process_table[slot].pgid = process_table[slot].pid;
    process_table[slot].state = PROC_STATE_READY;
    strncpy(process_table[slot].name, name, PROC_NAME_LEN - 1);
    process_table[slot].name[PROC_NAME_LEN - 1] = '\0';
    process_table[slot].start_time = pit_get_ticks();
    process_table[slot].cpu_time = 0;
    process_table[slot].priority = 10;
    process_table[slot].exit_code = 0;
    process_table[slot].waiting_for_pid = 0;
    init_process_fd_table(&process_table[slot]);
    init_process_signals(&process_table[slot]);
    
    return process_table[slot].pid;
}

int process_kill(uint32_t pid)
{
    if (pid == 0) {
        return -1;
    }
    if (pid == 1) {
        return -2;
    }
    
    process_t *proc = process_get(pid);
    if (!proc) {
        return -3;
    }
    
    proc->state = PROC_STATE_ZOMBIE;
    proc->state = PROC_STATE_UNUSED;
    
    return 0;
}

process_t *process_current(void)
{
    return process_get(current_pid);
}

void process_set_current(uint32_t pid)
{
    current_pid = pid;
}

const char *process_state_name(uint8_t state)
{
    if (state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "unknown";
}


int32_t process_fork(void)
{
    process_t *parent = process_current();
    if (!parent) return -1;
    
    uint32_t child_pid = process_create(parent->name, parent->pid);
    if (child_pid == 0) {
        return -11;  
    }
    
    process_t *child = process_get(child_pid);
    if (!child) return -1;
    
    child->pgid = parent->pgid;
    
    for (int i = 0; i < MAX_FDS_PER_PROC; i++) {
        child->fd_table[i] = parent->fd_table[i];
    }
    
    child->blocked_signals = parent->blocked_signals;
    for (int i = 0; i < NSIG; i++) {
        child->signal_handlers[i] = parent->signal_handlers[i];
    }
    
    serial_puts("[FORK] Created child PID ");
    char buf[12];
    int idx = 0;
    uint32_t n = child_pid;
    if (n == 0) buf[idx++] = '0';
    else { while (n > 0) { buf[idx++] = '0' + (n % 10); n /= 10; } }
    buf[idx] = '\0';
    for (int j = 0; j < idx / 2; j++) { char t = buf[j]; buf[j] = buf[idx-1-j]; buf[idx-1-j] = t; }
    serial_puts(buf);
    serial_puts("\n");
    
    return (int32_t)child_pid;
}


int32_t process_exec(const char *path, char *const argv[])
{
    (void)argv;
    
    process_t *proc = process_current();
    if (!proc) return -1;
    
    user_process_t *elf = elf_load_from_file(path);
    if (!elf) {
        return -2;  
    }
    
    if (proc->elf_proc) {
        elf_free_process((user_process_t *)proc->elf_proc);
    }
    if (proc->kernel_stack) {
        kfree((void *)proc->kernel_stack);
    }
    
    const char *name = path;
    const char *slash = path;
    while (*slash) {
        if (*slash == '/') name = slash + 1;
        slash++;
    }
    strncpy(proc->name, name, PROC_NAME_LEN - 1);
    proc->name[PROC_NAME_LEN - 1] = '\0';
    
    proc->pending_signals = 0;
    for (int i = 0; i < NSIG; i++) {
        proc->signal_handlers[i] = (sighandler_t)0;
    }
    
    for (int i = 3; i < MAX_FDS_PER_PROC; i++) {
        if (proc->fd_table[i].in_use && (proc->fd_table[i].flags & 0x80000000)) {
            proc->fd_table[i].in_use = 0;
            proc->fd_table[i].node = NULL;
        }
    }
    
    serial_puts("[EXEC] Executing ");
    serial_puts(path);
    serial_puts("\n");
    
    elf_execute(elf);
    
    return 0;
}


int32_t process_waitpid(int32_t pid, int32_t *status, int options)
{
    (void)options;
    
    process_t *parent = process_current();
    if (!parent) return -1;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *child = &process_table[i];
        
        if (child->state == PROC_STATE_UNUSED) continue;
        if (child->ppid != parent->pid) continue;
        if (pid > 0 && child->pid != (uint32_t)pid) continue;
        
        if (child->state == PROC_STATE_ZOMBIE) {
            int32_t child_pid = (int32_t)child->pid;
            
            if (status) {
                *status = child->exit_code;
            }
            
            child->state = PROC_STATE_UNUSED;
            
            serial_puts("[WAIT] Reaped child PID ");
            char buf[12];
            int idx = 0;
            uint32_t n = child_pid;
            if (n == 0) buf[idx++] = '0';
            else { while (n > 0) { buf[idx++] = '0' + (n % 10); n /= 10; } }
            buf[idx] = '\0';
            for (int j = 0; j < idx / 2; j++) { char t = buf[j]; buf[j] = buf[idx-1-j]; buf[idx-1-j] = t; }
            serial_puts(buf);
            serial_puts("\n");
            
            return child_pid;
        }
    }
    
    return -10;  
}

void process_exit(int32_t status)
{
    process_t *proc = process_current();
    if (!proc || proc->pid <= 1) return;
    
    serial_puts("[EXIT] Process exiting\n");
    
    proc->exit_code = status;
    process_reparent_children(proc->pid);
    process_signal(proc->ppid, SIGCHLD);
    proc->state = PROC_STATE_ZOMBIE;
}


int32_t process_setpgid(uint32_t pid, uint32_t pgid)
{
    process_t *proc = (pid == 0) ? process_current() : process_get(pid);
    if (!proc) return -3;
    
    proc->pgid = (pgid == 0) ? proc->pid : pgid;
    return 0;
}

int32_t process_getpgid(uint32_t pid)
{
    process_t *proc = (pid == 0) ? process_current() : process_get(pid);
    if (!proc) return -3;
    
    return (int32_t)proc->pgid;
}


int32_t process_signal(uint32_t pid, int sig)
{
    if (sig < 0 || sig >= NSIG) return -22;
    if (sig == 0) return process_get(pid) ? 0 : -3;
    
    process_t *proc = process_get(pid);
    if (!proc) return -3;
    
    proc->pending_signals |= (1 << (sig - 1));
    
    if (sig == SIGCONT && proc->state == PROC_STATE_STOPPED) {
        proc->state = PROC_STATE_READY;
    }
    
    return 0;
}

void process_check_signals(void)
{
    process_t *proc = process_current();
    if (!proc) return;
    
    uint32_t pending = proc->pending_signals & ~proc->blocked_signals;
    if (pending == 0) return;
    
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(pending & (1 << (sig - 1)))) continue;
        
        proc->pending_signals &= ~(1 << (sig - 1));
        
        sighandler_t handler = proc->signal_handlers[sig];
        
        if (sig == SIGKILL) {
            process_exit(-SIGKILL);
            return;
        }
        
        if (sig == SIGSTOP) {
            proc->state = PROC_STATE_STOPPED;
            return;
        }
        
        if (handler == (sighandler_t)0) {
            if (sig != SIGCHLD && sig != SIGURG && sig != SIGWINCH) {
                if (sig == SIGCONT) {
                    if (proc->state == PROC_STATE_STOPPED) {
                        proc->state = PROC_STATE_READY;
                    }
                } else if (sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU) {
                    proc->state = PROC_STATE_STOPPED;
                } else {
                    process_exit(-sig);
                    return;
                }
            }
        } else if (handler != (sighandler_t)1) {
            handler(sig);
        }
    }
}


void process_reparent_children(uint32_t parent_pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_UNUSED &&
            process_table[i].ppid == parent_pid) {
            process_table[i].ppid = 1;
            serial_puts("[ORPHAN] Reparented child to init\n");
        }
    }
}
