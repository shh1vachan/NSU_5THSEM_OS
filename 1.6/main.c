#include <stdio.h>
#include "mythread_lib.h"

static void *thread_func(void *arg)
{
    const char *msg = (const char *)arg;
    printf("mythread started, message: %s\n", msg);
    return "done";
}

int main(void)
{
    mythread_t t;
    void *retval = NULL;

    if (mythread_create(&t, thread_func, "hello from mythread") != MYTHREAD_OK) {
        fprintf(stderr, "mythread_create failed\n");
        return 1;
    }

    if (mythread_join(&t, &retval) != MYTHREAD_OK) {
        fprintf(stderr, "mythread_join failed\n");
        return 1;
    }

    printf("mythread finished, retval = %s\n", (char *)retval);
    return 0;
}
