#include <stdio.h>
#include "mythread_lib.h"

static void *thread_function(void *arg)
{
    const char *msg = (const char *)arg;
    printf("mythread started, message: %s\n", msg);
    return NULL;
}

int main(void)
{
    mythread_t t;
    int status;

    status = mythread_create(&t, thread_function, "hello from thread");
    if (status != MYTHREAD_SUCCESS) {
        fprintf(stderr, "mythread_create failed\n");
        return 1;
    }

    pthread_join(t.handle, NULL);

    return 0;
}
