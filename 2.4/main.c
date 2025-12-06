#define _GNU_SOURCE
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "lock.h"

typedef enum {
    MODE_MY_SPIN = 0,
    MODE_MY_MUTEX = 1,
    MODE_PTHREAD_SPIN = 2,
    MODE_PTHREAD_MUTEX = 3
} lock_mode_t;

static long long counter = 0;
static lock_t g_lock;
static pthread_mutex_t g_pmutex;
static pthread_spinlock_t g_pspin;
static lock_mode_t g_mode;

struct thread_arg {
    long iterations;
};

static void *worker(void *argp)
{
    struct thread_arg *arg = (struct thread_arg *)argp;

    for (long i = 0; i < arg->iterations; ++i) {
        switch (g_mode) {
        case MODE_MY_SPIN:
        case MODE_MY_MUTEX:
            lock_lock(&g_lock);
            counter++;
            lock_unlock(&g_lock);
            break;
        case MODE_PTHREAD_MUTEX:
            pthread_mutex_lock(&g_pmutex);
            counter++;
            pthread_mutex_unlock(&g_pmutex);
            break;
        case MODE_PTHREAD_SPIN:
            pthread_spin_lock(&g_pspin);
            counter++;
            pthread_spin_unlock(&g_pspin);
            break;
        }
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int  n_threads  = 4;
    long iterations = 1000000;
    const char *mode_str = "my_spin";

    g_mode = MODE_MY_SPIN;

    if (argc >= 2) {
        n_threads = atoi(argv[1]);
    }
    if (argc >= 3) {
        iterations = atol(argv[2]);
    }
    if (argc >= 4) {
        if (strcmp(argv[3], "spin") == 0) {
            g_mode = MODE_MY_SPIN;
            mode_str = "my_spin";
        } else if (strcmp(argv[3], "mutex") == 0) {
            g_mode = MODE_MY_MUTEX;
            mode_str = "my_mutex";
        } else if (strcmp(argv[3], "p_spin") == 0) {
            g_mode = MODE_PTHREAD_SPIN;
            mode_str = "pthread_spin";
        } else if (strcmp(argv[3], "p_mutex") == 0) {
            g_mode = MODE_PTHREAD_MUTEX;
            mode_str = "pthread_mutex";
        } else {
            fprintf(stderr, "Usage: %s [threads] [iterations] [spin|mutex|p_spin|p_mutex]\n", argv[0]);
            return 1;
        }
    }

    if (g_mode == MODE_MY_SPIN) {
        lock_init(&g_lock, LOCK_KIND_SPIN);
    } else if (g_mode == MODE_MY_MUTEX) {
        lock_init(&g_lock, LOCK_KIND_MUTEX);
    } else if (g_mode == MODE_PTHREAD_MUTEX) {
        pthread_mutex_init(&g_pmutex, NULL);
    } else if (g_mode == MODE_PTHREAD_SPIN) {
        pthread_spin_init(&g_pspin, PTHREAD_PROCESS_PRIVATE);
    }

    pthread_t *threads = malloc(sizeof(pthread_t) * n_threads);
    if (!threads) {
        perror("malloc");
        return 1;
    }

    struct thread_arg arg;
    arg.iterations = iterations;

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
           mode_str, n_threads, iterations, counter, elapsed);

    if (g_mode == MODE_PTHREAD_MUTEX) {
        pthread_mutex_destroy(&g_pmutex);
    } else if (g_mode == MODE_PTHREAD_SPIN) {
        pthread_spin_destroy(&g_pspin);
    }

    free(threads);
    return 0;
}
