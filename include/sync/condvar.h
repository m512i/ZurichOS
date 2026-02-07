#ifndef _SYNC_CONDVAR_H
#define _SYNC_CONDVAR_H

#include <stdint.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>
#include <sync/mutex.h>

typedef struct condvar {
    wait_queue_t waiters;
    spinlock_t lock;
} condvar_t;

#define CONDVAR_INIT { .waiters = WAIT_QUEUE_INIT, .lock = SPINLOCK_INIT }

void condvar_init(condvar_t *cv);
void condvar_wait(condvar_t *cv, mutex_t *mutex);
void condvar_signal(condvar_t *cv);
void condvar_broadcast(condvar_t *cv);

#endif
