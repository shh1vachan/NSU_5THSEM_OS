#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "lock.h"

static long long counter = 0;

static my_spinlock_t g_spin;
static my_mutex_t    g_mutex;

struct thread_arg {
    long iterations;
    int use_mutex;
};

static void *worker(void *argp)
{
    struct thread_arg *arg = (struct thread_arg *)argp;

    for (long i = 0; i < arg->iterations; ++i) {
        if (arg->use_mutex) {
            my_mutex_lock(&g_mutex);
            counter++;
            my_mutex_unlock(&g_mutex);
        } else {
            my_spin_lock(&g_spin);
            counter++;
            my_spin_unlock(&g_spin);
        }
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int  n_threads  = 4;
    long iterations = 1000000;
    int  use_mutex  = 0;

    if (argc >= 2) {
        n_threads = atoi(argv[1]);
    }
    if (argc >= 3) {
        iterations = atol(argv[2]);
    }
    if (argc >= 4) {
        if (strcmp(argv[3], "mutex") == 0) {
            use_mutex = 1;
        } else if (strcmp(argv[3], "spin") == 0) {
            use_mutex = 0;
        } else {
            fprintf(stderr, "Usage: %s [threads] [iterations] [spin|mutex]\n", argv[0]);
            return 1;
        }
    }

    if (use_mutex) {
        my_mutex_init(&g_mutex);
    } else {
        my_spin_init(&g_spin);
    }

    pthread_t *threads = malloc(sizeof(pthread_t) * n_threads);
    if (!threads) {
        perror("malloc");
        return 1;
    }

    struct thread_arg arg;
    arg.iterations = iterations;
    arg.use_mutex  = use_mutex;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < n_threads; ++i) {
        if (pthread_create(&threads[i], NULL, worker, &arg) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < n_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (t1.tv_sec - t0.tv_sec)
                   + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    printf("lock=%s threads=%d iter=%ld total_ops=%lld time=%.6f sec\n",
           use_mutex ? "my_mutex" : "my_spin",
           n_threads, iterations, counter, elapsed);

    free(threads);
    return 0;
}
