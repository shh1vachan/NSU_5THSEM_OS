#ifndef PROXY_H
#define PROXY_H

#include <pthread.h>

typedef struct client_ctx {
    int client_fd;
} client_ctx_t;

void *client_thread(void *arg);

#endif
