/* Scheduler 
 * Preemptive round-robin scheduler with context switching
 */

#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <arch/x86/gdt.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <drivers/serial.h>
#include <drivers/pit.h>
#include <apic/lapic.h>
#include <arch/x86/idt.h>
#include <string.h>

#define MAX_TASKS 64
static task_t tasks[MAX_TASKS];
static uint32_t next_tid = 1;

static task_t *current_task = NULL;
static task_t *idle_task = NULL;
static task_t *ready_queue_head = NULL;
static task_t *ready_queue_tail = NULL;

static volatile int scheduler_lock = 0;
static volatile int scheduler_enabled = 0;

#define DEFAULT_STACK_SIZE  4096

static void idle_task_func(void)
{
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void ready_queue_add(task_t *task)
{
    task->next = NULL;
    task->state = TASK_STATE_READY;
    
    if (ready_queue_tail) {
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    } else {
        ready_queue_head = ready_queue_tail = task;
    }
}

static task_t *ready_queue_pop(void)
{
    if (!ready_queue_head) {
        return NULL;
    }
    
    task_t *task = ready_queue_head;
    ready_queue_head = task->next;
    
    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }
    
    task->next = NULL;
    return task;
}

static task_t *alloc_task_slot(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_UNUSED) {
            return &tasks[i];
        }
    }
    return NULL;
}

void scheduler_init(void)
{
    serial_puts("[SCHED] Initializing scheduler\n");
    
    memset(tasks, 0, sizeof(tasks));
    
    task_t *kernel_task = &tasks[0];
    kernel_task->tid = 0;
    kernel_task->pid = 0;
    strcpy(kernel_task->name, "kernel");
    kernel_task->state = TASK_STATE_RUNNING;
    kernel_task->priority = 0;
    kernel_task->base_priority = 0;
    kernel_task->inherited_priority = 0;
    kernel_task->waiting_on = NULL;
    kernel_task->kernel_stack = NULL;
    kernel_task->kernel_stack_size = 0;
    kernel_task->esp = 0;
    kernel_task->page_directory = NULL;
    kernel_task->cpu_time = 0;
    kernel_task->next = NULL;
    
    current_task = kernel_task;
    
    idle_task = task_create("idle", idle_task_func, DEFAULT_STACK_SIZE);
    if (idle_task) {
        if (ready_queue_head == idle_task) {
            ready_queue_pop();
        }
        idle_task->state = TASK_STATE_READY;
        idle_task->priority = 255;
    }
    
    serial_puts("[SCHED] Scheduler initialized\n");
}

task_t *task_create(const char *name, void (*entry)(void), uint32_t stack_size)
{
    task_t *task = alloc_task_slot();
    if (!task) {
        serial_puts("[SCHED] No free task slots\n");
        return NULL;
    }
    
    if (stack_size == 0) {
        stack_size = DEFAULT_STACK_SIZE;
    }
    
    uint32_t *stack = (uint32_t *)kmalloc(stack_size);
    if (!stack) {
        serial_puts("[SCHED] Failed to allocate stack\n");
        return NULL;
    }
    
    task->tid = next_tid++;
    task->pid = current_task ? current_task->pid : 0;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_STATE_READY;
    task->priority = 10;
    task->base_priority = 10;
    task->inherited_priority = 0;
    task->waiting_on = NULL;
    task->kernel_stack = stack;
    task->kernel_stack_size = stack_size;
    task->page_directory = NULL;
    task->cpu_time = 0;
    task->wake_time = 0;
    task->next = NULL;
    
    uint32_t *sp = (uint32_t *)((uint32_t)stack + stack_size);
    
    *(--sp) = (uint32_t)task_exit;
    
    *(--sp) = (uint32_t)entry;
    *(--sp) = 0;  /* EBP */
    *(--sp) = 0;  /* EBX */
    *(--sp) = 0;  /* ESI */
    *(--sp) = 0;  /* EDI */
    
    task->esp = (uint32_t)sp;
    
    ready_queue_add(task);
    
    serial_puts("[SCHED] Created task: ");
    serial_puts(name);
    serial_puts("\n");
    
    return task;
}

task_t *task_current(void)
{
    return current_task;
}

void schedule(void)
{
    if (!scheduler_enabled || scheduler_lock) {
        return;
    }
    
    schedule_force();
}

void schedule_force(void)
{
    if (scheduler_lock) {
        return;
    }
    
    task_t *next = ready_queue_pop();
    
    if (!next) {
        return;
    }
    
    if (next == current_task) {
        return;  
    }
    
    task_t *prev = current_task;
    
    if (prev && prev->state == TASK_STATE_RUNNING && prev != idle_task) {
        ready_queue_add(prev);
    }
    
    current_task = next;
    next->state = TASK_STATE_RUNNING;
    
    if (next->kernel_stack) {
        uint32_t stack_top = (uint32_t)next->kernel_stack + next->kernel_stack_size;
        gdt_set_kernel_stack(stack_top);
    }
    
    if (prev) {
        context_switch(&prev->esp, next->esp);
    }
}

void scheduler_tick(void)
{
    if (!scheduler_enabled || !current_task) {
        return;
    }
    
    current_task->cpu_time++;
    
    uint64_t now;
    if (idt_is_apic_mode()) {
        now = lapic_get_uptime_ms();
    } else {
        now = pit_get_uptime_ms();
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_SLEEPING) {
            if (now >= tasks[i].wake_time) {
                ready_queue_add(&tasks[i]);
            }
        }
    }
    
    if (ready_queue_head && current_task->tid != 0) {
        schedule();
    }
}

void task_block(uint8_t reason)
{
    if (!current_task) return;
    
    current_task->state = TASK_STATE_BLOCKED;
    current_task->block_reason = reason;
    schedule_force(); 
}

void task_unblock(task_t *task)
{
    if (!task || task->state != TASK_STATE_BLOCKED) return;
    
    task->block_reason = BLOCK_REASON_NONE;
    ready_queue_add(task);
}

void task_sleep(uint32_t ms)
{
    if (!current_task) return;
    
    uint64_t now;
    if (idt_is_apic_mode()) {
        now = lapic_get_uptime_ms();
    } else {
        now = pit_get_uptime_ms();
    }
    
    current_task->wake_time = now + ms;
    current_task->state = TASK_STATE_SLEEPING;
    schedule();
}

void scheduler_enable(void)
{
    scheduler_enabled = 1;
    serial_puts("[SCHED] Scheduler enabled\n");
}

void scheduler_disable(void)
{
    scheduler_enabled = 0;
}

void task_exit(int status)
{
    (void)status;
    
    if (!current_task) return;
    
    serial_puts("[SCHED] Task exiting: ");
    serial_puts(current_task->name);
    serial_puts("\n");
    
    if (current_task->kernel_stack) {
        kfree(current_task->kernel_stack);
    }
    
    current_task->state = TASK_STATE_ZOMBIE;
    schedule_force();
    
    while (1) {
        __asm__ volatile("hlt");
    }
}

void task_set_priority(task_t *task, uint8_t priority)
{
    if (!task) return;
    task->base_priority = priority;
    task->priority = (task->inherited_priority > 0 && task->inherited_priority < priority) 
                     ? task->inherited_priority : priority;
}

uint8_t task_get_effective_priority(task_t *task)
{
    if (!task) return 255;
    return task->priority;
}

void task_boost_priority(task_t *task, uint8_t priority)
{
    if (!task) return;
    
    if (task->inherited_priority == 0 || priority < task->inherited_priority) {
        task->inherited_priority = priority;
    }
    
    if (priority < task->priority) {
        task->priority = priority;
    }
}

void task_restore_priority(task_t *task)
{
    if (!task) return;
    
    task->inherited_priority = 0;
    task->priority = task->base_priority;
}

int scheduler_get_task_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_STATE_UNUSED && tasks[i].tid != 0) {
            count++;
        }
    }
    return count;
}
