#ifndef UTHREAD_LIB_H
#define UTHREAD_LIB_H

#define UTHREAD_OK 0
#define UTHREAD_ERR 1
#define UTHREAD_MAX_THREADS 128

#include <stddef.h>
#include <ucontext.h>

typedef enum {
    UTHREAD_READY = 0,
    UTHREAD_RUNNING,
    UTHREAD_FINISHED
} uthread_state_t;

typedef struct uthread {
    int id;
    void *(*start_routine)(void *);
    void *arg;
    ucontext_t context;
    uthread_state_t state;
    void *retval;
} uthread_t;

int uthread_create(uthread_t *thread,
                   void *(*start_routine)(void *),
                   void *arg);

void uthread_run_all(void);

void scheduler(void);

#endif
