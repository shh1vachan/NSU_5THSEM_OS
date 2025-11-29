#include "lock.h"
#include <string.h>
#include <stdio.h>

LockMode global_lock_mode = LOCK_MODE_MUTEX;

void set_lock_mode(LockMode mode) {
    global_lock_mode = mode;
}

LockMode parse_lock_mode(const char *name) {
    if (!name) return LOCK_MODE_MUTEX;
    if (strcmp(name, "mutex") == 0) return LOCK_MODE_MUTEX;
    if (strcmp(name, "spin") == 0 || strcmp(name, "spinlock") == 0) return LOCK_MODE_SPIN;
    if (strcmp(name, "rw") == 0 || strcmp(name, "rwlock") == 0) return LOCK_MODE_RW;
    return LOCK_MODE_MUTEX;
}

const char *lock_mode_name(LockMode mode) {
    switch (mode) {
        case LOCK_MODE_MUTEX: return "mutex";
        case LOCK_MODE_SPIN:  return "spin";
        case LOCK_MODE_RW:    return "rwlock";
        default:              return "unknown";
    }
}

int node_lock_init(NodeLock *l) {
    l->mode = global_lock_mode;
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_init(&l->u.m, NULL);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_init(&l->u.s, PTHREAD_PROCESS_PRIVATE);
    } else {
        return pthread_rwlock_init(&l->u.rw, NULL);
    }
}

int node_lock_destroy(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_destroy(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_destroy(&l->u.s);
    } else {
        return pthread_rwlock_destroy(&l->u.rw);
    }
}

int node_lock_read(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_lock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_lock(&l->u.s);
    } else {
        return pthread_rwlock_rdlock(&l->u.rw);
    }
}

int node_unlock_read(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_unlock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_unlock(&l->u.s);
    } else {
        return pthread_rwlock_unlock(&l->u.rw);
    }
}

int node_lock_write(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_lock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_lock(&l->u.s);
    } else {
        return pthread_rwlock_wrlock(&l->u.rw);
    }
}

int node_unlock_write(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_unlock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return pthread_spin_unlock(&l->u.s);
    } else {
        return pthread_rwlock_unlock(&l->u.rw);
    }
}
