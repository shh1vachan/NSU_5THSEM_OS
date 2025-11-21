#include <stdio.h>
#include "mythread_lib.h"

static void *thread_func(void *arg)
{
    const char *msg = (const char *)arg;
    printf("thread says: %s\n", msg);
    return "done";
}

int main(void)
{
    mythread_t t;
    if (mythread_create(&t, thread_func, "hello from mythread") != MYTHREAD_SUCCESS) {
        fprintf(stderr, "mythread_create failed\n");
        return 1;
    }

    void *retval = NULL;
    if (mythread_join(&t, &retval) != MYTHREAD_SUCCESS) {
        fprintf(stderr, "mythread_join failed\n");
        return 1;
    }

    return 0;
}
