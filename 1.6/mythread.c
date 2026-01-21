#define _GNU_SOURCE
#include "mythread_lib.h"

#include <sched.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

static int futex_wait(atomic_int *addr, int expected)
{
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(atomic_int *addr, int n)
{
    return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

static int mythread_entry(void *arg)
{
    mythread_t *t = (mythread_t *)arg;

    t->retval = t->start_routine(t->arg);

    atomic_store_explicit(&t->completed, 1, memory_order_release);
    futex_wake(&t->completed, 1);

    return 0;
}

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        return MYTHREAD_ERR;
    }

    memset(thread, 0, sizeof(*thread));
    atomic_store(&thread->completed, 0);
    atomic_store(&thread->detached, 0);
    atomic_store(&thread->joined, 0);

    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->retval = NULL;

    thread->stack = mmap(NULL, STACK_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (thread->stack == MAP_FAILED) {
        thread->stack = NULL;
        return MYTHREAD_ERR;
    }

    void *child_stack = (char *)thread->stack + STACK_SIZE;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;

    if (clone(mythread_entry, child_stack, flags, thread) == -1) {
        munmap(thread->stack, STACK_SIZE);
        thread->stack = NULL;
        return MYTHREAD_ERR;
    }

    return MYTHREAD_OK;
}

int mythread_join(mythread_t *thread, void **retval)
{
    if (thread == NULL) {
        return MYTHREAD_ERR;
    }

    if (atomic_load_explicit(&thread->detached, memory_order_acquire)) {
        return MYTHREAD_ERR;
    }

    if (atomic_exchange_explicit(&thread->joined, 1, memory_order_acq_rel)) {
        return MYTHREAD_ERR;
    }

    while (!atomic_load_explicit(&thread->completed, memory_order_acquire)) {
        futex_wait(&thread->completed, 0);
    }

    if (retval) {
        *retval = thread->retval;
    }

    if (thread->stack) {
        munmap(thread->stack, STACK_SIZE);
        thread->stack = NULL;
    }

    return MYTHREAD_OK;
}

int mythread_detach(mythread_t *thread)
{
    if (thread == NULL) {
        return MYTHREAD_ERR;
    }

    atomic_store_explicit(&thread->detached, 1, memory_order_release);

    if (atomic_load_explicit(&thread->completed, memory_order_acquire) &&
        !atomic_exchange_explicit(&thread->joined, 1, memory_order_acq_rel)) {

        if (thread->stack) {
            munmap(thread->stack, STACK_SIZE);
            thread->stack = NULL;
        }
    }

    return MYTHREAD_OK;
}