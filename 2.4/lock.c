#define _GNU_SOURCE
#include "lock.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

static int futex_wait(int *addr, int expected)
{
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(int *addr, int n)
{
    return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

static inline void cpu_relax(void)
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    sched_yield();
#endif
}

void my_spin_init(my_spinlock_t *l)
{
    l->value = 0;
}

void my_spin_lock(my_spinlock_t *l)
{
    for (;;) {
        if (__sync_bool_compare_and_swap(&l->value, 0, 1)) {
            return;
        }

        for (int i = 0; i < 100; ++i) {
            if (__atomic_load_n(&l->value, __ATOMIC_RELAXED) == 0) {
                break;
            }
            cpu_relax();
        }
    }
}

int my_spin_trylock(my_spinlock_t *l)
{
    if (__sync_bool_compare_and_swap(&l->value, 0, 1)) {
        return 0;
    }
    return 1;
}

void my_spin_unlock(my_spinlock_t *l)
{
    int v = __atomic_load_n(&l->value, __ATOMIC_RELAXED);
    if (v == 0) {
        fprintf(stderr, "my_spin_unlock: unlocking unlocked spinlock\n");
        abort();
    }
    __atomic_store_n(&l->value, 0, __ATOMIC_RELEASE);
}

void my_mutex_init(my_mutex_t *m)
{
    m->value = 0;
}

void my_mutex_lock(my_mutex_t *m)
{
    for (;;) {
        if (__sync_bool_compare_and_swap(&m->value, 0, 1)) {
            return;
        }

        for (int i = 0; i < 100; ++i) {
            int v = __atomic_load_n(&m->value, __ATOMIC_RELAXED);
            if (v == 0) {
                break;
            }
            cpu_relax();
        }

        int old = __sync_val_compare_and_swap(&m->value, 1, 2);
        if (old == 0) {
            continue;
        }

        futex_wait(&m->value, 2);
    }
}

int my_mutex_trylock(my_mutex_t *m)
{
    if (__sync_bool_compare_and_swap(&m->value, 0, 1)) {
        return 0;
    }
    return 1;
}

void my_mutex_unlock(my_mutex_t *m)
{
    int v = __atomic_load_n(&m->value, __ATOMIC_RELAXED);
    if (v <= 0) {
        fprintf(stderr, "my_mutex_unlock: unlocking unlocked mutex\n");
        abort();
    }

    int prev = __sync_fetch_and_sub(&m->value, 1);

    if (prev == 1) {
        return;
    }

    __atomic_store_n(&m->value, 0, __ATOMIC_RELEASE);
    futex_wake(&m->value, 1);
}

void lock_init(lock_t *l, lock_kind_t kind)
{
    l->kind = kind;
    if (kind == LOCK_KIND_SPIN) {
        my_spin_init(&l->spin);
    } else {
        my_mutex_init(&l->mutex);
    }
}

void lock_lock(lock_t *l)
{
    if (l->kind == LOCK_KIND_SPIN) {
        my_spin_lock(&l->spin);
    } else {
        my_mutex_lock(&l->mutex);
    }
}

int lock_trylock(lock_t *l)
{
    if (l->kind == LOCK_KIND_SPIN) {
        return my_spin_trylock(&l->spin);
    } else {
        return my_mutex_trylock(&l->mutex);
    }
}

void lock_unlock(lock_t *l)
{
    if (l->kind == LOCK_KIND_SPIN) {
        my_spin_unlock(&l->spin);
    } else {
        my_mutex_unlock(&l->mutex);
    }
}
