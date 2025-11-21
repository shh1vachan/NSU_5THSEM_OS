#ifndef UTHREAD_LIB_H
#define UTHREAD_LIB_H

#define UTHREAD_OK 0
#define UTHREAD_ERR 1
#define UTHREAD_MAX_THREADS 128

#include <stddef.h>

typedef struct uthread {
    int id;
    void *(*start_routine)(void *);
    void *arg;
} uthread_t;

int uthread_create(uthread_t *thread,
                   void *(*start_routine)(void *),
                   void *arg);

void uthread_run_all(void);

#endif
