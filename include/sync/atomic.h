#ifndef _SYNC_ATOMIC_H
#define _SYNC_ATOMIC_H

#include <stdint.h>

typedef struct atomic {
    volatile int32_t value;
} atomic_t;

#define ATOMIC_INIT(val) { .value = (val) }

static inline int32_t atomic_read(const atomic_t *v)
{
    return v->value;
}

static inline void atomic_set(atomic_t *v, int32_t val)
{
    v->value = val;
}

static inline int32_t atomic_add(atomic_t *v, int32_t val)
{
    return __sync_add_and_fetch(&v->value, val);
}

static inline int32_t atomic_sub(atomic_t *v, int32_t val)
{
    return __sync_sub_and_fetch(&v->value, val);
}

static inline void atomic_inc(atomic_t *v)
{
    __sync_add_and_fetch(&v->value, 1);
}

static inline void atomic_dec(atomic_t *v)
{
    __sync_sub_and_fetch(&v->value, 1);
}

static inline int atomic_inc_and_test(atomic_t *v)
{
    return __sync_add_and_fetch(&v->value, 1) == 0;
}

static inline int atomic_dec_and_test(atomic_t *v)
{
    return __sync_sub_and_fetch(&v->value, 1) == 0;
}

static inline int32_t atomic_cmpxchg(atomic_t *v, int32_t old, int32_t new)
{
    return __sync_val_compare_and_swap(&v->value, old, new);
}

static inline int32_t atomic_xchg(atomic_t *v, int32_t new)
{
    return __sync_lock_test_and_set(&v->value, new);
}

static inline void memory_barrier(void)
{
    __asm__ volatile("mfence" : : : "memory");
}

static inline void read_barrier(void)
{
    __asm__ volatile("lfence" : : : "memory");
}

static inline void write_barrier(void)
{
    __asm__ volatile("sfence" : : : "memory");
}

#endif
