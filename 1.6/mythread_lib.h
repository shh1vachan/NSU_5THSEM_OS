#ifndef MYTHREAD_LIB_H
#define MYTHREAD_LIB_H

#include <pthread.h>

typedef struct {
    pthread_t handle;
} mythread_t;

#define MYTHREAD_SUCCESS 0
#define MYTHREAD_ERROR   1

// create analog
int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg);

#endif // MYTHREAD_LIB__H
