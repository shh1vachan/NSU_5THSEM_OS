#include "swapper.h"
#include "counter.h"
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

atomic_long swap_counts[3] = {0, 0, 0};

typedef int (*NeedSwapFn)(int l1, int l2);

static int need_swap_for_inc(int l1, int l2) {
    return l1 > l2;
}

static int need_swap_for_dec(int l1, int l2) {
    return l1 < l2;
}

static int need_swap_for_eq(int l1, int l2) {
    return l1 != l2;
}

static void do_random_swap_once(List *lst, NeedSwapFn fn, atomic_long *counter, unsigned int *seed) {
    if (lst->size < 2) return;

    int max_index = (int)lst->size - 1;
    if (max_index <= 0) return;

    int idx = rand_r(seed) % max_index;

    Node *prev = lst->head;
    if (!prev) return;

    if (node_lock_write(&prev->lock) != 0) return;

    Node *a = prev->next;
    if (!a) {
        node_unlock_write(&prev->lock);
        return;
    }
    node_lock_write(&a->lock);

    for (int i = 0; i < idx; ++i) {
        Node *next = a->next;
        if (!next) break;

        node_lock_write(&next->lock);

        node_unlock_write(&prev->lock);
        prev = a;
        a = next;
    }

    Node *b = a->next;
    if (!b) {
        node_unlock_write(&a->lock);
        node_unlock_write(&prev->lock);
        return;
    }
    node_lock_write(&b->lock);

    int l1 = (int)strlen(a->text);
    int l2 = (int)strlen(b->text);

    if (fn(l1, l2)) {
        prev->next = b;
        a->next = b->next;
        b->next = a;
        atomic_fetch_add(counter, 1);
    }

    node_unlock_write(&b->lock);
    node_unlock_write(&a->lock);
    node_unlock_write(&prev->lock);
}

void *swap_worker(void *arg) {
    SwapArg *a = (SwapArg *)arg;
    int id = a->id;
    NeedSwapFn fn = NULL;
    atomic_long *ctr = NULL;

    if (id == 0) {
        fn = need_swap_for_inc;
        ctr = &swap_counts[0];
    } else if (id == 1) {
        fn = need_swap_for_dec;
        ctr = &swap_counts[1];
    } else {
        fn = need_swap_for_eq;
        ctr = &swap_counts[2];
    }

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self();

    while (atomic_load(&running)) {
        do_random_swap_once(&global_list, fn, ctr, &seed);
        sched_yield();
    }

    return NULL;
}
