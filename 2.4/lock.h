#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>

typedef struct {
    int value;
} my_spinlock_t;

typedef struct {
    int value;
} my_mutex_t;

void my_spin_init(my_spinlock_t *l);
void my_spin_lock(my_spinlock_t *l);
void my_spin_unlock(my_spinlock_t *l);

void my_mutex_init(my_mutex_t *m);
void my_mutex_lock(my_mutex_t *m);
void my_mutex_unlock(my_mutex_t *m);

#endif
