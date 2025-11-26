#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Node *make_node(const char *s) {
    Node *n = malloc(sizeof(Node));
    if (!n) { perror("malloc"); exit(1); }
    strncpy(n->text, s, MAX_TEXT_LEN - 1);
    n->text[MAX_TEXT_LEN - 1] = '\0';
    n->next = NULL;
    pthread_mutex_init(&n->lock, NULL);
    return n;
}

void list_init_random(List *lst, size_t n) {
    lst->head = NULL;
    Node *prev = NULL;

    for (size_t i = 0; i < n; ++i) {
        int len = 1 + rand() % 20;
        char buf[MAX_TEXT_LEN];

        for (int j = 0; j < len; ++j)
            buf[j] = 'a' + (rand() % 26);
        buf[len] = '\0';

        Node *cur = make_node(buf);
        if (!lst->head) lst->head = cur;
        else prev->next = cur;
        prev = cur;
    }
}

void list_free(List *lst) {
    Node *p = lst->head;
    while (p) {
        Node *n = p->next;
        pthread_mutex_destroy(&p->lock);
        free(p);
        p = n;
    }
    lst->head = NULL;
}
