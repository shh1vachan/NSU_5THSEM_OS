#include "uthread_lib.h"
#include <stdlib.h>

static uthread_t *uthreads[UTHREAD_MAX_THREADS];
static int uthread_count = 0;

static ucontext_t scheduler_context;
static int current_thread = -1;

static void uthread_entry(void)
{
    uthread_t *t = uthreads[current_thread];
    t->retval = t->start_routine(t->arg);
    t->state = UTHREAD_FINISHED;
    swapcontext(&t->context, &scheduler_context);
}

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
    thread->state = UTHREAD_READY;
    thread->retval = NULL;

    getcontext(&thread->context);
    thread->context.uc_stack.ss_size = 64 * 1024;
    thread->context.uc_stack.ss_sp = malloc(thread->context.uc_stack.ss_size);
    if (thread->context.uc_stack.ss_sp == NULL) {
        return UTHREAD_ERR;
    }
    thread->context.uc_link = &scheduler_context;

    makecontext(&thread->context, uthread_entry, 0);

    uthreads[uthread_count] = thread;
    uthread_count++;

    return UTHREAD_OK;
}


void uthread_run_all(void)
{
    while (1) {
        int alive = 0;

        for (int i = 0; i < uthread_count; ++i) {
            uthread_t *t = uthreads[i];

            if (t->state == UTHREAD_FINISHED) {
                if (t->context.uc_stack.ss_sp != NULL) {
                    free(t->context.uc_stack.ss_sp);
                    t->context.uc_stack.ss_sp = NULL;
                }
                continue;
            }

            alive = 1;
            current_thread = i;
            t->state = UTHREAD_RUNNING;
            swapcontext(&scheduler_context, &t->context);
        }

        if (!alive) {
            break;
        }
    }

    current_thread = -1;
}


void scheduler(void)
{
    if (current_thread < 0) {
        return;
    }

    uthread_t *t = uthreads[current_thread];
    if (t->state == UTHREAD_RUNNING) {
        t->state = UTHREAD_READY;
    }

    swapcontext(&t->context, &scheduler_context);
}