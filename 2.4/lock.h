#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>

typedef struct {
    int value;
} my_spinlock_t;

typedef struct {
    int value;
} my_mutex_t;

typedef enum {
    LOCK_KIND_SPIN = 0,
    LOCK_KIND_MUTEX = 1
} lock_kind_t;

typedef struct {
    lock_kind_t kind;
    my_spinlock_t spin;
    my_mutex_t mutex;
} lock_t;

void my_spin_init(my_spinlock_t *l);
void my_spin_lock(my_spinlock_t *l);
int  my_spin_trylock(my_spinlock_t *l);
void my_spin_unlock(my_spinlock_t *l);

void my_mutex_init(my_mutex_t *m);
void my_mutex_lock(my_mutex_t *m);
int  my_mutex_trylock(my_mutex_t *m);
void my_mutex_unlock(my_mutex_t *m);

void lock_init(lock_t *l, lock_kind_t kind);
void lock_lock(lock_t *l);
int  lock_trylock(lock_t *l);
void lock_unlock(lock_t *l);

#endif