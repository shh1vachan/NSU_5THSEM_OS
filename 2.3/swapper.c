#include "swapper.h"
#include "counter.h"
#include <unistd.h>

atomic_long swap_counts[3] = {0, 0, 0};

void *swap_worker(void *arg) {
    SwapArg *a = (SwapArg *)arg;
    int id = a->id;

    while (atomic_load(&running)) {
        Node *cur = global_list.head;

        if (!cur) {
            usleep(1000);
            continue;
        }

        pthread_mutex_lock(&cur->lock);

        while (cur) {
            Node *nx = cur->next;

            if (!nx) {
                pthread_mutex_unlock(&cur->lock);
                break;
            }

            pthread_mutex_lock(&nx->lock);

            pthread_mutex_unlock(&cur->lock);
            cur = nx;
        }

        usleep(1000);
        (void)id;
    }

    return NULL;
}
