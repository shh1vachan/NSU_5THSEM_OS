#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>

#include "list.h"
#include "counter.h"
#include "swapper.h"
#include "lock.h"

#define DEFAULT_SIZE 1000
#define RUN_TIME 5

int main(int argc, char **argv) {
    size_t n = DEFAULT_SIZE;
    LockMode mode = LOCK_MODE_MUTEX;

    if (argc > 1) {
        size_t t = strtoul(argv[1], NULL, 10);
        if (t > 0) n = t;
    }
    if (argc > 2) {
        mode = parse_lock_mode(argv[2]);
    }

    set_lock_mode(mode);

    printf("list size = %zu, lock mode = %s\n", n, lock_mode_name(mode));
    fflush(stdout);

    list_init_random(&global_list, n);

    pthread_t t_inc, t_dec, t_eq;
    pthread_t t_sw[3];
    SwapArg args[3];

    pthread_create(&t_inc, NULL, thread_increasing, NULL);
    pthread_create(&t_dec, NULL, thread_decreasing, NULL);
    pthread_create(&t_eq,  NULL, thread_equal,      NULL);

    for (int i = 0; i < 3; ++i) {
        args[i].id = i;
        pthread_create(&t_sw[i], NULL, swap_worker, &args[i]);
    }

    sleep(RUN_TIME);
    atomic_store(&running, 0);

    pthread_join(t_inc, NULL);
    pthread_join(t_dec, NULL);
    pthread_join(t_eq,  NULL);

    for (int i = 0; i < 3; ++i)
        pthread_join(t_sw[i], NULL);

    printf("passes: inc=%ld dec=%ld eq=%ld\n",
           (long)atomic_load(&inc_passes),
           (long)atomic_load(&dec_passes),
           (long)atomic_load(&eq_passes));

    printf("swaps:  inc=%ld dec=%ld eq=%ld\n",
           (long)atomic_load(&swap_counts[0]),
           (long)atomic_load(&swap_counts[1]),
           (long)atomic_load(&swap_counts[2]));

    list_free(&global_list);
    return 0;
}
