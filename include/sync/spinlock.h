#ifndef _SYNC_SPINLOCK_H
#define _SYNC_SPINLOCK_H

#include <stdint.h>

typedef struct spinlock {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0 }

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);
int spinlock_try_acquire(spinlock_t *lock);
void spinlock_irq_save(spinlock_t *lock, uint32_t *flags);
void spinlock_irq_restore(spinlock_t *lock, uint32_t flags);

#endif
