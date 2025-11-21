#include "mythread_lib.h"
#include <stdlib.h>

struct mythread_start_wrapper {
    mythread_t *thread;
    void *(*user_routine)(void *);
    void *user_arg;
};

static void *mythread_trampoline(void *arg)
{
    struct mythread_start_wrapper *w = arg;
    mythread_t *t = w->thread;
    void *(*fn)(void *) = w->user_routine;
    void *user_arg = w->user_arg;
    free(w);

    atomic_store(&t->started, 1);

    void *ret = fn(user_arg);

    t->retval = ret;
    atomic_store(&t->finished, 1);

    return ret;
}

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        return MYTHREAD_ERROR;
    }

    atomic_store(&thread->started, 0);
    atomic_store(&thread->finished, 0);
    atomic_store(&thread->joined, 0);
    atomic_store(&thread->detached, 0);
    thread->retval = NULL;

    struct mythread_start_wrapper *w = malloc(sizeof(*w));
    if (!w) {
        return MYTHREAD_ERROR;
    }

    w->thread = thread;
    w->user_routine = start_routine;
    w->user_arg = arg;

    int rc = pthread_create(&thread->handle, NULL, mythread_trampoline, w);
    if (rc != 0) {
        free(w);
        return MYTHREAD_ERROR;
    }

    return MYTHREAD_SUCCESS;
}

int mythread_join(mythread_t *thread, void **retval)
{
    if (thread == NULL) {
        return MYTHREAD_ERROR;
    }

    if (atomic_load(&thread->detached)) {
        return MYTHREAD_ERROR;
    }

    int expected = 0;
    if (!atomic_compare_exchange_strong(&thread->joined, &expected, 1)) {
        return MYTHREAD_ERROR;
    }

    void *ret = NULL;
    int rc = pthread_join(thread->handle, &ret);
    if (rc != 0) {
        return MYTHREAD_ERROR;
    }

    if (retval) 
        *retval = thread->retval;

    return MYTHREAD_SUCCESS;
}

int mythread_detach(mythread_t *thread)
{
    if (thread == NULL) 
        return MYTHREAD_ERROR;

    if (atomic_load(&thread->joined)) 
        return MYTHREAD_ERROR;

    int expected = 0;
    if (!atomic_compare_exchange_strong(&thread->detached, &expected, 1)) {
        return MYTHREAD_ERROR;
    }

    int rc = pthread_detach(thread->handle);
    if (rc != 0) 
        return MYTHREAD_ERROR;

    return MYTHREAD_SUCCESS;
}
