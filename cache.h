#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>

typedef enum {
    CACHE_LOADING = 0,
    CACHE_READY,
    CACHE_ERROR
} cache_state_t;

typedef struct cache_entry {
    char *key;
    char *origin_host;
    char *origin_port;
    char *req;
    size_t req_len;

    unsigned char *data;
    size_t size;
    size_t capacity;

    cache_state_t state;

    int refcount;

    pthread_mutex_t mtx;
    pthread_cond_t cond;

    struct cache_entry *hnext;
    struct cache_entry *lru_prev;
    struct cache_entry *lru_next;
} cache_entry_t;

void cache_init(void);
void cache_set_max_bytes(size_t max_bytes);

cache_entry_t *cache_get_or_create(const char *key,
                                   const char *origin_host,
                                   const char *origin_port,
                                   const char *req_buf,
                                   size_t req_len,
                                   int *is_new);

void cache_entry_add_data(cache_entry_t *e, const void *data, size_t len);
void cache_entry_mark_ready(cache_entry_t *e);
void cache_entry_mark_error(cache_entry_t *e);

ssize_t cache_entry_read(cache_entry_t *e, size_t *offset,
                         void *buf, size_t buf_sz, int *finished);

void cache_entry_acquire(cache_entry_t *e);
void cache_entry_release(cache_entry_t *e);

#endif
