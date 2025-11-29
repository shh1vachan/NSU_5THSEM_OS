#ifndef COUNTER_H
#define COUNTER_H

#include <stdatomic.h>
#include "list.h"

extern List global_list;

extern atomic_long inc_passes;
extern atomic_long dec_passes;
extern atomic_long eq_passes;

extern atomic_int running;

void *thread_increasing(void *arg);
void *thread_decreasing(void *arg);
void *thread_equal(void *arg);

#endif
