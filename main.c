#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "proxy.h"
#include "cache.h"

static int create_listen_socket(const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(NULL, port, &hints, &res);
    if (rc != 0) return -1;

    int fd = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            if (listen(fd, 1024) == 0) break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

int main(int argc, char **argv) {
    const char *port = "80";
    if (argc >= 2 && argv[1] && argv[1][0]) port = argv[1];

    signal(SIGPIPE, SIG_IGN);

    cache_init();
    cache_set_max_bytes((size_t)1 << 30);

    int listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        return 1;
    }

    for (;;) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int cfd = accept(listen_fd, (struct sockaddr *)&ss, &slen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            continue;
        }

        client_ctx_t *ctx = (client_ctx_t *)calloc(1, sizeof(client_ctx_t));
        if (!ctx) { close(cfd); continue; }
        ctx->client_fd = cfd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, ctx) == 0) {
            pthread_detach(tid);
        } else {
            close(cfd);
            free(ctx);
        }
    }

    close(listen_fd);
    return 0;
}
