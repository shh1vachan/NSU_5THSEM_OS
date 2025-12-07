#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "proxy.h"

static int listen_fd = -1;

static void handle_sigint(int sig) {
    (void)sig;
    if (listen_fd != -1) {
        close(listen_fd);
    }
    fprintf(stderr, "\n[INFO] proxy shutting down\n");
    _exit(0);
}

static int create_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv) {
    int port = 8080;

    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        return 1;
    }

    fprintf(stderr, "[INFO] listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        fprintf(stderr, "[INFO] new connection from %s:%d\n",
                ip_str, ntohs(client_addr.sin_port));

        client_ctx_t *ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            fprintf(stderr, "[ERR] malloc failed\n");
            close(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, client_thread, ctx);
        if (rc != 0) {
            fprintf(stderr, "[ERR] pthread_create: %s\n", strerror(rc));
            close(client_fd);
            free(ctx);
            continue;
        }

        pthread_detach(tid);
    }

    return 0;
}
