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
        atomic_flag_clear(&l->u.spin.flag);
        return 0;
    } else {
        int r = pthread_mutex_init(&l->u.rw.m, NULL);
        if (r != 0) return r;
        r = pthread_cond_init(&l->u.rw.can_read, NULL);
        if (r != 0) return r;
        r = pthread_cond_init(&l->u.rw.can_write, NULL);
        if (r != 0) return r;
        l->u.rw.readers = 0;
        l->u.rw.writer = 0;
        return 0;
    }
}

int node_lock_destroy(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_destroy(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return 0;
    } else {
        int r1 = pthread_mutex_destroy(&l->u.rw.m);
        int r2 = pthread_cond_destroy(&l->u.rw.can_read);
        int r3 = pthread_cond_destroy(&l->u.rw.can_write);
        return r1 ? r1 : (r2 ? r2 : r3);
    }
}

static int spin_lock(atomic_flag *f) {
    while (atomic_flag_test_and_set_explicit(f, memory_order_acquire)) {
    }
    return 0;
}

static int spin_unlock(atomic_flag *f) {
    atomic_flag_clear_explicit(f, memory_order_release);
    return 0;
}

int node_lock_read(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_lock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return spin_lock(&l->u.spin.flag);
    } else {
        int r = pthread_mutex_lock(&l->u.rw.m);
        if (r != 0) return r;
        while (l->u.rw.writer) {
            pthread_cond_wait(&l->u.rw.can_read, &l->u.rw.m);
        }
        l->u.rw.readers++;
        r = pthread_mutex_unlock(&l->u.rw.m);
        return r;
    }
}

int node_unlock_read(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_unlock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return spin_unlock(&l->u.spin.flag);
    } else {
        int r = pthread_mutex_lock(&l->u.rw.m);
        if (r != 0) return r;
        l->u.rw.readers--;
        if (l->u.rw.readers == 0) {
            pthread_cond_signal(&l->u.rw.can_write);
        }
        r = pthread_mutex_unlock(&l->u.rw.m);
        return r;
    }
}

int node_lock_write(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_lock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return spin_lock(&l->u.spin.flag);
    } else {
        int r = pthread_mutex_lock(&l->u.rw.m);
        if (r != 0) return r;
        while (l->u.rw.writer || l->u.rw.readers > 0) {
            pthread_cond_wait(&l->u.rw.can_write, &l->u.rw.m);
        }
        l->u.rw.writer = 1;
        r = pthread_mutex_unlock(&l->u.rw.m);
        return r;
    }
}

int node_unlock_write(NodeLock *l) {
    if (l->mode == LOCK_MODE_MUTEX) {
        return pthread_mutex_unlock(&l->u.m);
    } else if (l->mode == LOCK_MODE_SPIN) {
        return spin_unlock(&l->u.spin.flag);
    } else {
        int r = pthread_mutex_lock(&l->u.rw.m);
        if (r != 0) return r;
        l->u.rw.writer = 0;
        pthread_cond_broadcast(&l->u.rw.can_read);
        pthread_cond_signal(&l->u.rw.can_write);
        r = pthread_mutex_unlock(&l->u.rw.m);
        return r;
    }
}
