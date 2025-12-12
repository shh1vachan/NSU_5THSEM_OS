#define _GNU_SOURCE
#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static cache_entry_t *g_head = NULL;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

#define MAX_OBJECT_SIZE (50 * 1024 * 1024)

void cache_init(void) {
}

static cache_entry_t *cache_find_locked(const char *key) {
    cache_entry_t *e = g_head;
    while (e) {
        if (strcmp(e->key, key) == 0)
            return e;
        e = e->next;
    }
    return NULL;
}

cache_entry_t *cache_get_or_create(const char *key,
                                   const char *origin_host,
                                   const char *origin_port,
                                   const char *req_buf,
                                   size_t req_len,
                                   int *is_new) {
    pthread_mutex_lock(&g_mtx);

    cache_entry_t *e = cache_find_locked(key);
    if (e) {
        e->refcount++;
        *is_new = 0;
        pthread_mutex_unlock(&g_mtx);
        return e;
    }

    e = calloc(1, sizeof(*e));
    if (!e) {
        pthread_mutex_unlock(&g_mtx);
        return NULL;
    }

    e->key = strdup(key);
    e->origin_host = strdup(origin_host);
    e->origin_port = strdup(origin_port);
    e->req = malloc(req_len);
    if (!e->key || !e->origin_host || !e->origin_port || !e->req) {
        free(e->key);
        free(e->origin_host);
        free(e->origin_port);
        free(e->req);
        free(e);
        pthread_mutex_unlock(&g_mtx);
        return NULL;
    }
    memcpy(e->req, req_buf, req_len);
    e->req_len = req_len;

    pthread_mutex_init(&e->mtx, NULL);
    pthread_cond_init(&e->cond, NULL);

    e->state = CACHE_LOADING;
    e->refcount = 1;
    e->data = NULL;
    e->size = 0;
    e->capacity = 0;

    e->next = g_head;
    g_head = e;

    *is_new = 1;
    pthread_mutex_unlock(&g_mtx);
    return e;
}

void cache_entry_add_data(cache_entry_t *e, const void *data, size_t len) {
    pthread_mutex_lock(&e->mtx);

    if (e->state != CACHE_LOADING) {
        pthread_mutex_unlock(&e->mtx);
        return;
    }

    if (e->size + len > MAX_OBJECT_SIZE) {
        e->state = CACHE_ERROR;
        pthread_cond_broadcast(&e->cond);
        pthread_mutex_unlock(&e->mtx);
        return;
    }

    size_t need = e->size + len;
    if (need > e->capacity) {
        size_t new_cap = e->capacity ? e->capacity * 2 : 8192;
        while (new_cap < need)
            new_cap *= 2;
        unsigned char *new_data = realloc(e->data, new_cap);
        if (!new_data) {
            e->state = CACHE_ERROR;
            pthread_cond_broadcast(&e->cond);
            pthread_mutex_unlock(&e->mtx);
            return;
        }
        e->data = new_data;
        e->capacity = new_cap;
    }

    memcpy(e->data + e->size, data, len);
    e->size += len;

    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

void cache_entry_mark_ready(cache_entry_t *e) {
    pthread_mutex_lock(&e->mtx);
    e->state = CACHE_READY;
    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

void cache_entry_mark_error(cache_entry_t *e) {
    pthread_mutex_lock(&e->mtx);
    e->state = CACHE_ERROR;
    pthread_cond_broadcast(&e->cond);
    pthread_mutex_unlock(&e->mtx);
}

ssize_t cache_entry_read(cache_entry_t *e, size_t *offset,
                         void *buf, size_t buf_sz, int *finished) {
    pthread_mutex_lock(&e->mtx);

    for (;;) {
        if (*offset < e->size) {
            size_t avail = e->size - *offset;
            size_t n = buf_sz < avail ? buf_sz : avail;
            memcpy(buf, e->data + *offset, n);
            *offset += n;
            *finished = 0;
            pthread_mutex_unlock(&e->mtx);
            return (ssize_t)n;
        }

        if (e->state == CACHE_READY || e->state == CACHE_ERROR) {
            *finished = 1;
            pthread_mutex_unlock(&e->mtx);
            return 0;
        }

        pthread_cond_wait(&e->cond, &e->mtx);
    }
}

void cache_entry_release(cache_entry_t *e) {
    pthread_mutex_lock(&e->mtx);
    if (e->refcount > 0)
        e->refcount--;
    pthread_mutex_unlock(&e->mtx);
}
