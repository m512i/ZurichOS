#ifndef _SYNC_MUTEX_H
#define _SYNC_MUTEX_H

#include <stdint.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>

struct task;

typedef struct mutex {
    volatile uint32_t locked;
    uint32_t owner;
    struct task *owner_task;
    spinlock_t lock;
    wait_queue_t waiters;
} mutex_t;

#define MUTEX_INIT { .locked = 0, .owner = 0, .owner_task = NULL, .lock = SPINLOCK_INIT, .waiters = WAIT_QUEUE_INIT }

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
int mutex_is_locked(mutex_t *mutex);

#endif
