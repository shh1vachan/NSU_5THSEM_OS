#define _GNU_SOURCE
#include "mythread_lib.h"

#include <sched.h>      
#include <stdlib.h>    
#include <sys/wait.h>   
#include <signal.h>     

struct mythread_start {
    mythread_t *thread;
    void *(*start_routine)(void *);
    void *arg;
};

static int mythread_trampoline(void *arg)
{
    struct mythread_start *ctx = arg;
    mythread_t *t = ctx->thread;
    void *(*fn)(void *) = ctx->start_routine;
    void *fn_arg = ctx->arg;

    free(ctx);

    void *ret = fn(fn_arg);

    t->retval  = ret;
    t->finished = 1;

    return 0;
}

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        return MYTHREAD_ERR;
    }

    size_t stack_size = 64 * 1024;
    void *stack = malloc(stack_size);
    if (!stack) {
        return MYTHREAD_ERR;
    }

    struct mythread_start *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        free(stack);
        return MYTHREAD_ERR;
    }

    ctx->thread = thread;
    ctx->start_routine = start_routine;
    ctx->arg = arg;

    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->finished = 0;
    thread->retval = NULL;

    void *stack_top = (char *)stack + stack_size;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD;

    pid_t tid = clone(mythread_trampoline, stack_top, flags, ctx);
    if (tid == -1) {
        free(ctx);
        free(stack);
        return MYTHREAD_ERR;
    }

    thread->tid = tid;
    return MYTHREAD_OK;
}

int mythread_join(mythread_t *thread, void **retval)
{
    if (thread == NULL) {
        return MYTHREAD_ERR;
    }

    int status = 0;
    pid_t r = waitpid(thread->tid, &status, 0);
    if (r == -1) {
        return MYTHREAD_ERR;
    }

    if (retval) {
        *retval = thread->retval;
    }

    free(thread->stack);
    thread->stack = NULL;
    thread->stack_size = 0;

    return MYTHREAD_OK;
}
