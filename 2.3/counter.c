#include "counter.h"
#include <string.h>

List global_list = { .head = NULL };

atomic_long inc_passes = 0;
atomic_long dec_passes = 0;
atomic_long eq_passes  = 0;

atomic_int running = 1;

typedef enum {
    CMP_INC,
    CMP_DEC,
    CMP_EQ
} CompareMode;

static long count_pairs(CompareMode mode) {
    long cnt = 0;
    Node *cur = global_list.head;

    if (!cur) return 0;
    pthread_mutex_lock(&cur->lock);

    while (cur) {
        Node *nx = cur->next;
        if (!nx) {
            pthread_mutex_unlock(&cur->lock);
            break;
        }

        pthread_mutex_lock(&nx->lock);

        size_t a = strlen(cur->text);
        size_t b = strlen(nx->text);

        int ok = 0;
        if (mode == CMP_INC) ok = (a < b);
        else if (mode == CMP_DEC) ok = (a > b);
        else ok = (a == b);

        if (ok) cnt++;

        pthread_mutex_unlock(&cur->lock);
        cur = nx;
    }

    return cnt;
}

void *thread_increasing(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs(CMP_INC);
        (void)x;
        atomic_fetch_add(&inc_passes, 1);
    }
    return NULL;
}

void *thread_decreasing(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs(CMP_DEC);
        (void)x;
        atomic_fetch_add(&dec_passes, 1);
    }
    return NULL;
}

void *thread_equal(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs(CMP_EQ);
        (void)x;
        atomic_fetch_add(&eq_passes, 1);
    }
    return NULL;
}
