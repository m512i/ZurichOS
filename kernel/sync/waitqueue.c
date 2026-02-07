/* Wait Queue
 * Simple linked list of waiting tasks with blocking support
 */

#include <kernel/kernel.h>
#include <sync/waitqueue.h>
#include <kernel/scheduler.h>
#include <mm/heap.h>

void waitqueue_init(wait_queue_t *wq)
{
    wq->head = NULL;
    wq->tail = NULL;
    spinlock_init(&wq->lock);
}

void waitqueue_wait(wait_queue_t *wq)
{
    task_t *current = task_current();
    if (!current) return;
    
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    if (!entry) return;
    
    entry->task = current;
    entry->next = NULL;
    
    uint32_t flags;
    spinlock_irq_save(&wq->lock, &flags);
    
    if (wq->tail) {
        wq->tail->next = entry;
        wq->tail = entry;
    } else {
        wq->head = wq->tail = entry;
    }
    
    spinlock_irq_restore(&wq->lock, flags);
    task_block(0);
}

void waitqueue_wake_one(wait_queue_t *wq)
{
    uint32_t flags;
    spinlock_irq_save(&wq->lock, &flags);
    
    if (wq->head) {
        wait_queue_entry_t *entry = wq->head;
        wq->head = entry->next;
        
        if (!wq->head) {
            wq->tail = NULL;
        }
        
        task_t *task = entry->task;
        kfree(entry);
        
        spinlock_irq_restore(&wq->lock, flags);
        
        if (task) {
            task_unblock(task);
        }
    } else {
        spinlock_irq_restore(&wq->lock, flags);
    }
}

void waitqueue_wake_all(wait_queue_t *wq)
{
    uint32_t flags;
    spinlock_irq_save(&wq->lock, &flags);
    
    while (wq->head) {
        wait_queue_entry_t *entry = wq->head;
        wq->head = entry->next;
        
        task_t *task = entry->task;
        kfree(entry);
        
        if (task) {
            task_unblock(task);
        }
    }
    
    wq->tail = NULL;
    spinlock_irq_restore(&wq->lock, flags);
}

int waitqueue_empty(wait_queue_t *wq)
{
    return wq->head == NULL;
}
