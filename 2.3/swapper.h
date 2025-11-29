#ifndef SWAPPER_H
#define SWAPPER_H

#include <stdatomic.h>
#include "list.h"

typedef struct {
    int id;
} SwapArg;

extern atomic_long swap_counts[3];

void *swap_worker(void *arg);

#endif
