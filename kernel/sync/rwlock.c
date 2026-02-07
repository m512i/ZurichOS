/* Read-Write Lock
 * Multiple readers OR single writer access
 */

#include <kernel/kernel.h>
#include <sync/rwlock.h>
#include <sync/waitqueue.h>
#include <kernel/scheduler.h>

void rwlock_init(rwlock_t *rw)
{
    rw->readers = 0;
    rw->writer = 0;
    rw->writer_waiting = 0;
    spinlock_init(&rw->lock);
    waitqueue_init(&rw->read_waiters);
    waitqueue_init(&rw->write_waiters);
}

void rwlock_read_lock(rwlock_t *rw)
{
    for (;;) {
        uint32_t flags;
        spinlock_irq_save(&rw->lock, &flags);
        
        if (!rw->writer && !rw->writer_waiting) {
            rw->readers++;
            spinlock_irq_restore(&rw->lock, flags);
            return;
        }
        
        spinlock_irq_restore(&rw->lock, flags);
        waitqueue_wait(&rw->read_waiters);
    }
}

void rwlock_read_unlock(rwlock_t *rw)
{
    uint32_t flags;
    spinlock_irq_save(&rw->lock, &flags);
    
    rw->readers--;
    
    if (rw->readers == 0 && rw->writer_waiting) {
        spinlock_irq_restore(&rw->lock, flags);
        waitqueue_wake_one(&rw->write_waiters);
    } else {
        spinlock_irq_restore(&rw->lock, flags);
    }
}

void rwlock_write_lock(rwlock_t *rw)
{
    for (;;) {
        uint32_t flags;
        spinlock_irq_save(&rw->lock, &flags);
        
        if (!rw->writer && rw->readers == 0) {
            rw->writer = 1;
            rw->writer_waiting = 0;
            spinlock_irq_restore(&rw->lock, flags);
            return;
        }
        
        rw->writer_waiting = 1;
        spinlock_irq_restore(&rw->lock, flags);
        waitqueue_wait(&rw->write_waiters);
    }
}

void rwlock_write_unlock(rwlock_t *rw)
{
    uint32_t flags;
    spinlock_irq_save(&rw->lock, &flags);
    
    rw->writer = 0;
    
    int has_write_waiters = !waitqueue_empty(&rw->write_waiters);
    int has_read_waiters = !waitqueue_empty(&rw->read_waiters);
    
    spinlock_irq_restore(&rw->lock, flags);
    
    if (has_read_waiters) {
        waitqueue_wake_all(&rw->read_waiters);
    }
    
    if (has_write_waiters && !has_read_waiters) {
        waitqueue_wake_one(&rw->write_waiters);
    }
}

int rwlock_try_read_lock(rwlock_t *rw)
{
    uint32_t flags;
    spinlock_irq_save(&rw->lock, &flags);
    
    if (!rw->writer && !rw->writer_waiting) {
        rw->readers++;
        spinlock_irq_restore(&rw->lock, flags);
        return 1;
    }
    
    spinlock_irq_restore(&rw->lock, flags);
    return 0;
}

int rwlock_try_write_lock(rwlock_t *rw)
{
    uint32_t flags;
    spinlock_irq_save(&rw->lock, &flags);
    
    if (!rw->writer && rw->readers == 0) {
        rw->writer = 1;
        spinlock_irq_restore(&rw->lock, flags);
        return 1;
    }
    
    spinlock_irq_restore(&rw->lock, flags);
    return 0;
}
