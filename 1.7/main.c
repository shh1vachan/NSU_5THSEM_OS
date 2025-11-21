#include <stdio.h>
#include "uthread_lib.h"

static void *worker1(void *arg)
{
    (void)arg;
    printf("uthread 1: hello!\n");
    return NULL;
}

static void *worker2(void *arg)
{
    const char *msg = (const char *)arg;
    printf("uthread 2: %s\n", msg);
    return NULL;
}

int main(void)
{
    uthread_t t1;
    uthread_t t2;

    if (uthread_create(&t1, worker1, NULL) != UTHREAD_OK) {
        fprintf(stderr, "uthread_create t1 failed\n");
        return 1;
    }

    if (uthread_create(&t2, worker2, "hi from user thread") != UTHREAD_OK) {
        fprintf(stderr, "uthread_create t2 failed\n");
        return 1;
    }

    uthread_run_all();

    return 0;
}
