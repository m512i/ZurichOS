/* Semaphore 
 * Counting semaphore for resource management with wait queue
 */

#include <kernel/kernel.h>
#include <sync/semaphore.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>

void semaphore_init(semaphore_t *sem, int32_t count)
{
    sem->count = count;
    spinlock_init(&sem->lock);
    waitqueue_init(&sem->waiters);
}

void semaphore_wait(semaphore_t *sem)
{
    for (;;) {
        uint32_t flags;
        spinlock_irq_save(&sem->lock, &flags);
        
        if (sem->count > 0) {
            sem->count--;
            spinlock_irq_restore(&sem->lock, flags);
            return;
        }
        
        spinlock_irq_restore(&sem->lock, flags);
        
        waitqueue_wait(&sem->waiters);
    }
}

int semaphore_trywait(semaphore_t *sem)
{
    uint32_t flags;
    spinlock_irq_save(&sem->lock, &flags);
    
    if (sem->count > 0) {
        sem->count--;
        spinlock_irq_restore(&sem->lock, flags);
        return 1;
    }
    
    spinlock_irq_restore(&sem->lock, flags);
    return 0;
}

void semaphore_signal(semaphore_t *sem)
{
    uint32_t flags;
    spinlock_irq_save(&sem->lock, &flags);
    
    sem->count++;
    
    spinlock_irq_restore(&sem->lock, flags);
    waitqueue_wake_one(&sem->waiters);
}

int32_t semaphore_get_count(semaphore_t *sem)
{
    return sem->count;
}
