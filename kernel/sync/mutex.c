/* Mutex
 * Blocking mutex with owner tracking and wait queue
 */

#include <kernel/kernel.h>
#include <sync/mutex.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>
#include <kernel/scheduler.h>

void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner = 0;
    mutex->owner_task = NULL;
    spinlock_init(&mutex->lock);
    waitqueue_init(&mutex->waiters);
}

void mutex_lock(mutex_t *mutex)
{
    task_t *current = task_current();
    uint32_t tid = current ? current->tid : 0;
    
    for (;;) {
        uint32_t flags;
        spinlock_irq_save(&mutex->lock, &flags);
        
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner = tid;
            mutex->owner_task = current;
            if (current) {
                current->waiting_on = NULL;
            }
            spinlock_irq_restore(&mutex->lock, flags);
            return;
        }
        
        if (current && mutex->owner_task) {
            uint8_t current_prio = task_get_effective_priority(current);
            uint8_t owner_prio = task_get_effective_priority(mutex->owner_task);
            
            if (current_prio < owner_prio) {
                task_boost_priority(mutex->owner_task, current_prio);
            }
            
            current->waiting_on = mutex;
        }
        
        spinlock_irq_restore(&mutex->lock, flags);
        
        waitqueue_wait(&mutex->waiters);
    }
}

void mutex_unlock(mutex_t *mutex)
{
    uint32_t flags;
    spinlock_irq_save(&mutex->lock, &flags);
    
    task_t *owner = mutex->owner_task;
    
    mutex->owner = 0;
    mutex->owner_task = NULL;
    mutex->locked = 0;
    
    if (owner) {
        task_restore_priority(owner);
    }
    
    spinlock_irq_restore(&mutex->lock, flags);
    
    waitqueue_wake_one(&mutex->waiters);
}

int mutex_trylock(mutex_t *mutex)
{
    task_t *current = task_current();
    uint32_t tid = current ? current->tid : 0;
    
    uint32_t flags;
    spinlock_irq_save(&mutex->lock, &flags);
    
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = tid;
        mutex->owner_task = current;
        spinlock_irq_restore(&mutex->lock, flags);
        return 1;
    }
    
    spinlock_irq_restore(&mutex->lock, flags);
    return 0;
}

int mutex_is_locked(mutex_t *mutex)
{
    return mutex->locked != 0;
}
