#define _GNU_SOURCE
#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#define HASH_BUCKETS 2048

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static cache_entry_t *g_table[HASH_BUCKETS];

static cache_entry_t *g_lru_head = NULL;
static cache_entry_t *g_lru_tail = NULL;

static size_t g_total_bytes = 0;
static size_t g_max_bytes = (size_t)1 << 30;
static size_t g_max_object_bytes = (size_t)1 << 30;

static unsigned long hash_key(const char *s) {
    unsigned long h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ull;
    }
    return h;
}

static void lru_remove(cache_entry_t *e) {
    if (!e) return;
    if (e->lru_prev) e->lru_prev->lru_next = e->lru_next;
    if (e->lru_next) e->lru_next->lru_prev = e->lru_prev;
    if (g_lru_head == e) g_lru_head = e->lru_next;
    if (g_lru_tail == e) g_lru_tail = e->lru_prev;
    e->lru_prev = e->lru_next = NULL;
}

static void lru_push_front(cache_entry_t *e) {
    if (!e) return;
    e->lru_prev = NULL;
    e->lru_next = g_lru_head;
    if (g_lru_head) g_lru_head->lru_prev = e;
    g_lru_head = e;
    if (!g_lru_tail) g_lru_tail = e;
}

static void lru_touch_locked(cache_entry_t *e) {
    lru_remove(e);
    lru_push_front(e);
}

static void free_entry_memory(cache_entry_t *e) {
    if (!e) return;
    free(e->key);
    free(e->origin_host);
    free(e->origin_port);
    free(e->req);
    free(e->data);
    pthread_mutex_destroy(&e->mtx);
    pthread_cond_destroy(&e->cond);
    free(e);
}

static void table_remove_locked(cache_entry_t *e) {
    if (!e || !e->key) return;
    unsigned long h = hash_key(e->key) % HASH_BUCKETS;
    cache_entry_t **pp = &g_table[h];
    while (*pp) {
        if (*pp == e) {
            *pp = e->hnext;
            e->hnext = NULL;
            break;
        }
        pp = &(*pp)->hnext;
    }
    lru_remove(e);
}

static int can_evict_locked(cache_entry_t *e) {
    return e && e->state == CACHE_READY && e->refcount == 0;
}

static void evict_if_needed_locked(size_t incoming) {
    if (incoming > g_max_bytes) return;
    while (g_total_bytes + incoming > g_max_bytes) {
        cache_entry_t *victim = g_lru_tail;
        while (victim && !can_evict_locked(victim)) {
            victim = victim->lru_prev;
        }
        if (!victim) break;
        table_remove_locked(victim);
        g_total_bytes -= victim->size;
        free_entry_memory(victim);
    }
}

void cache_set_max_bytes(size_t max_bytes) {
    pthread_mutex_lock(&g_mtx);
    g_max_bytes = max_bytes;
    if (g_max_object_bytes > g_max_bytes) g_max_object_bytes = g_max_bytes;
    evict_if_needed_locked(0);
    pthread_mutex_unlock(&g_mtx);
}

void cache_init(void) {
    pthread_mutex_lock(&g_mtx);
    memset(g_table, 0, sizeof(g_table));
    g_lru_head = g_lru_tail = NULL;
    g_total_bytes = 0;
    pthread_mutex_unlock(&g_mtx);
}

static cache_entry_t *table_find_locked(const char *key) {
    unsigned long h = hash_key(key) % HASH_BUCKETS;
    for (cache_entry_t *e = g_table[h]; e; e = e->hnext) {
        if (strcmp(e->key, key) == 0) return e;
    }
    return NULL;
}

static void table_insert_locked(cache_entry_t *e) {
    unsigned long h = hash_key(e->key) % HASH_BUCKETS;
    e->hnext = g_table[h];
    g_table[h] = e;
    lru_push_front(e);
}

void cache_entry_acquire(cache_entry_t *e) {
    if (!e) return;
    pthread_mutex_lock(&g_mtx);
    e->refcount++;
    pthread_mutex_unlock(&g_mtx);
}

static void maybe_cleanup_error_locked(cache_entry_t *e) {
    if (!e) return;
    if (e->state != CACHE_ERROR) return;
    if (e->refcount != 0) return;
    table_remove_locked(e);
    if (g_total_bytes >= e->size) g_total_bytes -= e->size;
    free_entry_memory(e);
}

void cache_entry_release(cache_entry_t *e) {
    if (!e) return;
    pthread_mutex_lock(&g_mtx);
    if (e->refcount > 0) e->refcount--;
    maybe_cleanup_error_locked(e);
    pthread_mutex_unlock(&g_mtx);
}

cache_entry_t *cache_get_or_create(const char *key,
                                   const char *origin_host,
                                   const char *origin_port,
                                   const char *req_buf,
                                   size_t req_len,
                                   int *is_new) {
    if (is_new) *is_new = 0;
    pthread_mutex_lock(&g_mtx);

    cache_entry_t *e = table_find_locked(key);
    if (e) {
        e->refcount++;
        lru_touch_locked(e);
        pthread_mutex_unlock(&g_mtx);
        return e;
    }

    evict_if_needed_locked(0);

    e = (cache_entry_t *)calloc(1, sizeof(cache_entry_t));
    if (!e) {
        pthread_mutex_unlock(&g_mtx);
        return NULL;
    }

    e->key = strdup(key);
    e->origin_host = strdup(origin_host);
    e->origin_port = strdup(origin_port);
    e->req = (char *)malloc(req_len);
    if (!e->key || !e->origin_host || !e->origin_port || !e->req) {
        free_entry_memory(e);
        pthread_mutex_unlock(&g_mtx);
        return NULL;
    }
    memcpy(e->req, req_buf, req_len);
    e->req_len = req_len;

    e->state = CACHE_LOADING;
    e->refcount = 1;

    pthread_mutex_init(&e->mtx, NULL);
    pthread_cond_init(&e->cond, NULL);

    table_insert_locked(e);
    if (is_new) *is_new = 1;

    pthread_mutex_unlock(&g_mtx);
    return e;
}

static int reserve_capacity(cache_entry_t *e, size_t needed) {
    if (needed <= e->capacity) return 0;

    size_t newcap = e->capacity ? e->capacity : 65536;
    while (newcap < needed) {
        if (newcap > (size_t)1 << 29) {
            newcap = needed;
            break;
        }
        newcap *= 2;
    }

    unsigned char *p = (unsigned char *)realloc(e->data, newcap);
    if (!p) return -1;
    e->data = p;
    e->capacity = newcap;
    return 0;
}

void cache_entry_add_data(cache_entry_t *e, const void *data, size_t len) {
    if (!e || !data || len == 0) return;

    pthread_mutex_lock(&e->mtx);

    if (e->state != CACHE_LOADING) {
        pthread_mutex_unlock(&e->mtx);
        return;
    }

    if (e->size + len > g_max_object_bytes) {
        e->state = CACHE_ERROR;
        pthread_cond_broadcast(&e->cond);
        pthread_mutex_unlock(&e->mtx);
        return;
    }

    size_t old_size = e->size;
    size_t new_size = e->size + len;

    pthread_mutex_lock(&g_mtx);
    evict_if_needed_locked(len);
    pthread_mutex_unlock(&g_mtx);

    if (reserve_capacity(e, new_size) < 0) {
        e->state = CACHE_ERROR;
        pthread_cond_broadcast(&e->cond);
        pthread_mutex_unlock(&e->mtx);
        return;
    }

    memcpy(e->data + e->size, data, len);
    e->size = new_size;

    pthread_mutex_lock(&g_mtx);
    g_total_bytes += (e->size - old_size);
    lru_touch_locked(e);
    pthread_mutex_unlock(&g_mtx);

    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

void cache_entry_mark_ready(cache_entry_t *e) {
    if (!e) return;
    pthread_mutex_lock(&e->mtx);
    if (e->state == CACHE_LOADING) e->state = CACHE_READY;
    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

void cache_entry_mark_error(cache_entry_t *e) {
    if (!e) return;
    pthread_mutex_lock(&e->mtx);
    e->state = CACHE_ERROR;
    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

ssize_t cache_entry_read(cache_entry_t *e, size_t *offset,
                         void *buf, size_t buf_sz, int *finished) {
    if (!e || !offset || !buf || buf_sz == 0) return -1;

    pthread_mutex_lock(&e->mtx);

    while (e->state == CACHE_LOADING && *offset >= e->size) {
        pthread_cond_wait(&e->cond, &e->mtx);
    }

    if (e->state == CACHE_ERROR && *offset >= e->size) {
        if (finished) *finished = 1;
        pthread_mutex_unlock(&e->mtx);
        return 0;
    }

    size_t avail = (e->size > *offset) ? (e->size - *offset) : 0;
    if (avail == 0) {
        if (finished) *finished = (e->state != CACHE_LOADING);
        pthread_mutex_unlock(&e->mtx);
        return 0;
    }

    size_t n = (avail < buf_sz) ? avail : buf_sz;
    memcpy(buf, e->data + *offset, n);
    *offset += n;

    if (finished) *finished = (e->state != CACHE_LOADING && *offset >= e->size);

    pthread_mutex_unlock(&e->mtx);

    pthread_mutex_lock(&g_mtx);
    lru_touch_locked(e);
    pthread_mutex_unlock(&g_mtx);

    return (ssize_t)n;
}
