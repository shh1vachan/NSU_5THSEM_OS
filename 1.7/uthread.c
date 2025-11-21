#include "uthread_lib.h"

static uthread_t *uthreads[UTHREAD_MAX_THREADS];
static int uthread_count = 0;

int uthread_create(uthread_t *thread,
                   void *(*start_routine)(void *),
                   void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        return UTHREAD_ERR;
    }

    if (uthread_count >= UTHREAD_MAX_THREADS) {
        return UTHREAD_ERR;
    }

    thread->id = uthread_count;
    thread->start_routine = start_routine;
    thread->arg = arg;

    uthreads[uthread_count] = thread;
    uthread_count++;

    return UTHREAD_OK;
}

void uthread_run_all(void)
{
    for (int i = 0; i < uthread_count; ++i) {
        uthread_t *t = uthreads[i];
        if (t && t->start_routine) {
            t->start_routine(t->arg);
        }
    }
}
