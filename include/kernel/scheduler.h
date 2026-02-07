#ifndef _KERNEL_SCHEDULER_H
#define _KERNEL_SCHEDULER_H

#include <stdint.h>

typedef struct cpu_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
} cpu_context_t;

struct mutex;

typedef struct task {
    uint32_t tid;
    uint32_t pid;
    char name[32];
    
    uint8_t state;
    uint8_t priority;
    uint8_t base_priority;
    uint8_t inherited_priority;
    uint8_t block_reason;
    
    struct mutex *waiting_on;
    
    uint32_t *kernel_stack;
    uint32_t kernel_stack_size;
    uint32_t esp;
    
    uint32_t *page_directory;
    
    uint64_t cpu_time;
    uint64_t wake_time;
    
    struct task *next;
} task_t;

#define BLOCK_REASON_NONE       0
#define BLOCK_REASON_MUTEX      1
#define BLOCK_REASON_SEMAPHORE  2
#define BLOCK_REASON_CONDVAR    3
#define BLOCK_REASON_IO         4
#define BLOCK_REASON_WAITQUEUE  5

#define TASK_STATE_UNUSED   0
#define TASK_STATE_RUNNING  1
#define TASK_STATE_READY    2
#define TASK_STATE_BLOCKED  3
#define TASK_STATE_SLEEPING 4
#define TASK_STATE_ZOMBIE   5

void scheduler_init(void);
void scheduler_enable(void);
void scheduler_disable(void);

task_t *task_create(const char *name, void (*entry)(void), uint32_t stack_size);
task_t *task_current(void);

void schedule(void);
void schedule_force(void);
void scheduler_tick(void);

void task_block(uint8_t reason);
void task_unblock(task_t *task);
void task_sleep(uint32_t ms);
void task_exit(int status);
void task_set_priority(task_t *task, uint8_t priority);
uint8_t task_get_effective_priority(task_t *task);
void task_boost_priority(task_t *task, uint8_t priority);
void task_restore_priority(task_t *task);
int scheduler_get_task_count(void);
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif
