#ifndef MYTHREAD_LIB_H
#define MYTHREAD_LIB_H

#include <pthread.h>
#include <stdatomic.h>

#define MYTHREAD_SUCCESS 0
#define MYTHREAD_ERROR   1

typedef struct {
    pthread_t handle;
    _Atomic int started;
    _Atomic int finished;
    _Atomic int joined;
    _Atomic int detached;
    void *retval;
} mythread_t;

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg);

int mythread_join(mythread_t *thread, void **retval);
int mythread_detach(mythread_t *thread);

#endif // MYTHREAD_LIB__H
