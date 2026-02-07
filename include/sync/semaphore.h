#ifndef _SYNC_SEMAPHORE_H
#define _SYNC_SEMAPHORE_H

#include <stdint.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>

typedef struct semaphore {
    volatile int32_t count;
    spinlock_t lock;
    wait_queue_t waiters;
} semaphore_t;

#define SEMAPHORE_INIT(n) { .count = (n), .lock = SPINLOCK_INIT, .waiters = WAIT_QUEUE_INIT }

void semaphore_init(semaphore_t *sem, int32_t count);
void semaphore_wait(semaphore_t *sem);
int semaphore_trywait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);
int32_t semaphore_get_count(semaphore_t *sem);

#endif 