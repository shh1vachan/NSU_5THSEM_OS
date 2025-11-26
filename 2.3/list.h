#ifndef LIST_H
#define LIST_H

#include <pthread.h>
#include <stddef.h>

#define MAX_TEXT_LEN 100

typedef struct Node {
    char text[MAX_TEXT_LEN];
    struct Node *next;
    pthread_mutex_t lock;
} Node;

typedef struct {
    Node *head;
} List;

void list_init_random(List *lst, size_t n);
void list_free(List *lst);

#endif
