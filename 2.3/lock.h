#ifndef LOCK_H
#define LOCK_H

#include <pthread.h>
#include <stdatomic.h>

typedef enum {
    LOCK_MODE_MUTEX,
    LOCK_MODE_SPIN,
    LOCK_MODE_RW
} LockMode;

typedef struct {
    LockMode mode;
    union {
        pthread_mutex_t m;
        struct {
            atomic_flag flag;
        } spin;
        struct {
            pthread_mutex_t m;
            pthread_cond_t can_read;
            pthread_cond_t can_write;
            int readers;
            int writer;
        } rw;
    } u;
} NodeLock;

extern LockMode global_lock_mode;

void set_lock_mode(LockMode mode);
LockMode parse_lock_mode(const char *name);
const char *lock_mode_name(LockMode mode);

int node_lock_init(NodeLock *l);
int node_lock_destroy(NodeLock *l);

int node_lock_read(NodeLock *l);
int node_unlock_read(NodeLock *l);
int node_lock_write(NodeLock *l);
int node_unlock_write(NodeLock *l);

#endif
