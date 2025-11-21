#include "mythread_lib.h"


int mythread_create(mythread_t *thread,
                    void *(*start_routine)(void *),
                    void *arg)
{
    if (thread == NULL || start_routine == NULL) 
    {
        return MYTHREAD_ERROR;
    }

    int rc = pthread_create(&thread->handle, NULL, start_routine, arg);
    if (rc != 0) 
    {
        return MYTHREAD_ERROR;
    }

    return MYTHREAD_SUCCESS;
}
