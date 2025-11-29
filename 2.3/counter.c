#include "counter.h"
#include <string.h>

List global_list = { .head = NULL, .size = 0 };

atomic_long inc_passes = 0;
atomic_long dec_passes = 0;
atomic_long eq_passes  = 0;

atomic_int running = 1;

typedef enum {
    CMP_INC,
    CMP_DEC,
    CMP_EQ
} CompareMode;

static long count_pairs_once(CompareMode mode) {
    Node *first = global_list.head;
    if (!first) return 0;

    Node *a;
    Node *b;
    long cnt = 0;

    if (node_lock_read(&first->lock) != 0) return 0;
    a = first->next;
    if (!a) {
        node_unlock_read(&first->lock);
        return 0;
    }
    node_lock_read(&a->lock);
    node_unlock_read(&first->lock);

    while (1) {
        b = a->next;
        if (!b) break;

        node_lock_read(&b->lock);

        size_t la = strlen(a->text);
        size_t lb = strlen(b->text);

        int ok = 0;
        if (mode == CMP_INC) ok = (la < lb);
        else if (mode == CMP_DEC) ok = (la > lb);
        else ok = (la == lb);

        if (ok) cnt++;

        node_unlock_read(&a->lock);
        a = b;
    }

    node_unlock_read(&a->lock);
    return cnt;
}

void *thread_increasing(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs_once(CMP_INC);
        (void)x;
        atomic_fetch_add(&inc_passes, 1);
    }
    return NULL;
}

void *thread_decreasing(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs_once(CMP_DEC);
        (void)x;
        atomic_fetch_add(&dec_passes, 1);
    }
    return NULL;
}

void *thread_equal(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        long x = count_pairs_once(CMP_EQ);
        (void)x;
        atomic_fetch_add(&eq_passes, 1);
    }
    return NULL;
}
