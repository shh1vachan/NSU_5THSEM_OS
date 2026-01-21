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
    mythread_t t1;
    mythread_t t2;

    if (mythread_create(&t1, thread_function, "join-thread") != MYTHREAD_OK) {
        fprintf(stderr, "mythread_create t1 failed\n");
        return 1;
    }

    if (mythread_create(&t2, thread_function, "detached-thread") != MYTHREAD_OK) {
        fprintf(stderr, "mythread_create t2 failed\n");
        return 1;
    }

    mythread_detach(&t2);
    mythread_join(&t1, NULL);

    return 0;
}