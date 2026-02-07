/* Spinlock
 * Basic spinlock with PAUSE instruction for efficiency
 */

#include <kernel/kernel.h>
#include <sync/spinlock.h>

void spinlock_init(spinlock_t *lock)
{
    lock->locked = 0;
}

void spinlock_acquire(spinlock_t *lock)
{
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ volatile("pause");
    }
}

void spinlock_release(spinlock_t *lock)
{
    __sync_lock_release(&lock->locked);
}

int spinlock_try_acquire(spinlock_t *lock)
{
    return !__sync_lock_test_and_set(&lock->locked, 1);
}

void spinlock_irq_save(spinlock_t *lock, uint32_t *flags)
{
    __asm__ volatile(
        "pushfl\n"
        "popl %0\n"
        "cli\n"
        : "=r"(*flags)
    );
    
    spinlock_acquire(lock);
}

void spinlock_irq_restore(spinlock_t *lock, uint32_t flags)
{
    spinlock_release(lock);
    
    __asm__ volatile(
        "pushl %0\n"
        "popfl\n"
        : : "r"(flags)
    );
}
