#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
 
#include "list.h"
#include "counter.h"
#include "swapper.h"

#define DEFAULT_SIZE 1000
#define RUN_TIME 5

int main(int argc, char **argv) {
    size_t n = DEFAULT_SIZE;
    if (argc > 1) {
        size_t t = strtoul(argv[1], NULL, 10);
        if (t > 0) n = t;
    }

    srand((unsigned)time(NULL));

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

    printf("inc_passes = %ld\n", (long)atomic_load(&inc_passes));
    printf("dec_passes = %ld\n", (long)atomic_load(&dec_passes));
    printf("eq_passes  = %ld\n", (long)atomic_load(&eq_passes));

    list_free(&global_list);
    return 0;
}
