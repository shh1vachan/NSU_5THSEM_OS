#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include "lock.h"

#define MAX_TEXT_LEN 100

typedef struct Node {
    char text[MAX_TEXT_LEN];
    struct Node *next;
    NodeLock lock;
} Node;

typedef struct {
    Node *head;
    size_t size;
} List;

void list_init_random(List *lst, size_t n);
void list_free(List *lst);

#endif
