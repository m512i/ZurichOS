/* Condition Variable
 * Allows threads to wait for a condition while holding a mutex
 */

#include <kernel/kernel.h>
#include <sync/condvar.h>
#include <sync/mutex.h>
#include <sync/waitqueue.h>
#include <kernel/scheduler.h>
#include <mm/heap.h>

void condvar_init(condvar_t *cv)
{
    waitqueue_init(&cv->waiters);
    spinlock_init(&cv->lock);
}

void condvar_wait(condvar_t *cv, mutex_t *mutex)
{
    task_t *current = task_current();
    if (!current) return;
    
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    if (!entry) return;
    
    entry->task = current;
    entry->next = NULL;
    
    uint32_t flags;
    spinlock_irq_save(&cv->lock, &flags);
    
    if (cv->waiters.tail) {
        cv->waiters.tail->next = entry;
        cv->waiters.tail = entry;
    } else {
        cv->waiters.head = cv->waiters.tail = entry;
    }
    
    spinlock_irq_restore(&cv->lock, flags);
    
    mutex_unlock(mutex);
    
    task_block(0);
    
    mutex_lock(mutex);
}

void condvar_signal(condvar_t *cv)
{
    uint32_t flags;
    spinlock_irq_save(&cv->lock, &flags);
    
    if (cv->waiters.head) {
        wait_queue_entry_t *entry = cv->waiters.head;
        cv->waiters.head = entry->next;
        
        if (!cv->waiters.head) {
            cv->waiters.tail = NULL;
        }
        
        task_t *task = entry->task;
        kfree(entry);
        
        spinlock_irq_restore(&cv->lock, flags);
        
        if (task) {
            task_unblock(task);
        }
    } else {
        spinlock_irq_restore(&cv->lock, flags);
    }
}

void condvar_broadcast(condvar_t *cv)
{
    uint32_t flags;
    spinlock_irq_save(&cv->lock, &flags);
    
    while (cv->waiters.head) {
        wait_queue_entry_t *entry = cv->waiters.head;
        cv->waiters.head = entry->next;
        
        task_t *task = entry->task;
        kfree(entry);
        
        if (task) {
            task_unblock(task);
        }
    }
    
    cv->waiters.tail = NULL;
    spinlock_irq_restore(&cv->lock, flags);
}
