#define _GNU_SOURCE
#include "lock.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static int futex_wait(int *addr, int expected)
{
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(int *addr, int n)
{
    return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
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

        while (__atomic_load_n(&l->value, __ATOMIC_RELAXED) != 0) {
        }
    }
}

void my_spin_unlock(my_spinlock_t *l)
{
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

        int old = __sync_val_compare_and_swap(&m->value, 1, 2);
        if (old == 0) {
            continue;
        }

        futex_wait(&m->value, 2);
    }
}

void my_mutex_unlock(my_mutex_t *m)
{
    int prev = __sync_fetch_and_sub(&m->value, 1);

    if (prev == 1) {
        return;
    }

    __atomic_store_n(&m->value, 0, __ATOMIC_RELEASE);
    futex_wake(&m->value, 1);
}
