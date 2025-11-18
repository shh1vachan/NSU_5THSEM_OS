#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <pthread.h>

typedef pthread_t mythread_t;

int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg);

#endif
