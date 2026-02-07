#ifndef _SYNC_WAITQUEUE_H
#define _SYNC_WAITQUEUE_H

#include <stdint.h>
#include <sync/spinlock.h>

struct task;

typedef struct wait_queue_entry {
    struct task *task;
    struct wait_queue_entry *next;
} wait_queue_entry_t;

typedef struct wait_queue {
    wait_queue_entry_t *head;
    wait_queue_entry_t *tail;
    spinlock_t lock;
} wait_queue_t;

#define WAIT_QUEUE_INIT { .head = NULL, .tail = NULL, .lock = SPINLOCK_INIT }

void waitqueue_init(wait_queue_t *wq);
void waitqueue_wait(wait_queue_t *wq);
void waitqueue_wake_one(wait_queue_t *wq);
void waitqueue_wake_all(wait_queue_t *wq);
int waitqueue_empty(wait_queue_t *wq);

#endif
