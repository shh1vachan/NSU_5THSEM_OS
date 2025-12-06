#ifndef MYTHREAD_LIB_H
#define MYTHREAD_LIB_H

#include <sys/types.h>
#include <stddef.h>

#define MYTHREAD_OK  0
#define MYTHREAD_ERR 1

typedef struct mythread {
    pid_t  tid;
    void  *stack;
    size_t stack_size;
    int    finished;
    void  *retval;
} mythread_t;

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg);

int mythread_join(mythread_t *thread, void **retval);

#endif
