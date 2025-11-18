#include "mythread.h"

int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg)
{
    return pthread_create(thread, NULL, start_routine, arg);
}
