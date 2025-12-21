#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static Node *make_node(const char *s) {
    Node *n = malloc(sizeof(Node));
    if (!n) {
        perror("malloc");
        exit(1);
    }
    if (s) {
        strncpy(n->text, s, MAX_TEXT_LEN - 1);
        n->text[MAX_TEXT_LEN - 1] = '\0';
    } else {
        n->text[0] = '\0';
        n->text[1] = '\0';
    }
    n->next = NULL;
    if (node_lock_init(&n->lock) != 0) {
        perror("lock init");
        exit(1);
    }
    return n;
}

void list_init_random(List *lst, size_t n) {
    lst->head = NULL;
    lst->size = 0;

    Node *sentinel = make_node(NULL);
    lst->head = sentinel;

    srand((unsigned)time(NULL));

    Node *prev = sentinel;

    for (size_t i = 0; i < n; ++i) {
        int len = 1 + rand() % (MAX_TEXT_LEN - 1);
        char buf[MAX_TEXT_LEN];

        for (int j = 0; j < len; ++j) {
            buf[j] = 'a' + (rand() % 26);
        }
        buf[len] = '\0';

        Node *cur = make_node(buf);
        prev->next = cur;
        prev = cur;
    }

    lst->size = n;
}

void list_free(List *lst) {
    Node *p = lst->head;
    while (p) {
        Node *n = p->next;
        node_lock_destroy(&p->lock);
        free(p);
        p = n;
    }
    lst->head = NULL;
    lst->size = 0;
}
