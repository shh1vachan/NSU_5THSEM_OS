#define _GNU_SOURCE
#include "net.h"

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;

    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return -1;
        }
        if (n == 0) {
            break;
        }
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)(len - left);
}

int connect_to_origin(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL, *rp;
    int fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "[ERR] getaddrinfo(%s,%s): %s\n",
                host, port, gai_strerror(rc));
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}
