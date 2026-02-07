#ifndef _KERNEL_PROCESS_H
#define _KERNEL_PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define PROC_STATE_UNUSED   0
#define PROC_STATE_RUNNING  1
#define PROC_STATE_READY    2
#define PROC_STATE_BLOCKED  3
#define PROC_STATE_ZOMBIE   4
#define PROC_STATE_STOPPED  5

#define MAX_PROCESSES       64
#define PROC_NAME_LEN       32
#define MAX_FDS_PER_PROC    32
#define NSIG                32

struct vfs_node;

typedef struct {
    struct vfs_node *node;
    uint32_t offset;
    uint32_t flags;
    int in_use;
    int pipe_id;
} fd_entry_t;

typedef void (*sighandler_t)(int);

typedef struct process {
    uint32_t pid;
    uint32_t ppid;
    uint32_t pgid;
    uint8_t state;
    char name[PROC_NAME_LEN];
    uint32_t start_time;
    uint32_t cpu_time;
    uint32_t priority;
    uint32_t kernel_stack;
    void *elf_proc;
    int32_t exit_code;
    uint32_t pending_signals;
    uint32_t blocked_signals;
    sighandler_t signal_handlers[NSIG];
    uint32_t saved_eax, saved_ebx, saved_ecx, saved_edx;
    uint32_t saved_esi, saved_edi, saved_ebp, saved_esp;
    uint32_t saved_eip, saved_eflags;
    uint32_t saved_cs, saved_ds, saved_es, saved_fs, saved_gs, saved_ss;
    uint32_t page_directory;
    uint32_t waiting_for_pid;
    
    fd_entry_t fd_table[MAX_FDS_PER_PROC];
} process_t;

void process_init(void);

process_t *process_get(uint32_t pid);
uint32_t process_count(void);
process_t *process_iterate(uint32_t *index);
uint32_t process_create(const char *name, uint32_t ppid);
int process_kill(uint32_t pid);
process_t *process_current(void);
void process_set_current(uint32_t pid);
const char *process_state_name(uint8_t state);

int32_t process_fork(void);
int32_t process_exec(const char *path, char *const argv[]);
int32_t process_waitpid(int32_t pid, int32_t *status, int options);
void process_exit(int32_t status);

int32_t process_setpgid(uint32_t pid, uint32_t pgid);
int32_t process_getpgid(uint32_t pid);

int32_t process_signal(uint32_t pid, int sig);
void process_check_signals(void);

void process_reparent_children(uint32_t parent_pid);

#endif
