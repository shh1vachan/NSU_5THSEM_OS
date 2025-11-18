#include <stdio.h>
#include "mythread.h"

void *thread_func(void *arg)
{
    printf("Hello from mythread! arg = %s\n", (char *)arg);
    return NULL;
}

int main(void)
{
    mythread_t t;
    mythread_create(&t, thread_func, "test");

    pthread_join(t, NULL);

    return 0;
}
