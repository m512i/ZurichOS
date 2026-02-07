#ifndef _SYNC_RWLOCK_H
#define _SYNC_RWLOCK_H

#include <stdint.h>
#include <sync/spinlock.h>
#include <sync/waitqueue.h>

typedef struct rwlock {
    volatile int32_t readers;
    volatile int32_t writer;
    volatile int32_t writer_waiting;
    spinlock_t lock;
    wait_queue_t read_waiters;
    wait_queue_t write_waiters;
} rwlock_t;

#define RWLOCK_INIT { .readers = 0, .writer = 0, .writer_waiting = 0, \
                      .lock = SPINLOCK_INIT, \
                      .read_waiters = WAIT_QUEUE_INIT, \
                      .write_waiters = WAIT_QUEUE_INIT }

void rwlock_init(rwlock_t *rw);
void rwlock_read_lock(rwlock_t *rw);
void rwlock_read_unlock(rwlock_t *rw);
void rwlock_write_lock(rwlock_t *rw);
void rwlock_write_unlock(rwlock_t *rw);
int rwlock_try_read_lock(rwlock_t *rw);
int rwlock_try_write_lock(rwlock_t *rw);

#endif
