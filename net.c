#define _GNU_SOURCE
#include "net.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/time.h>

static int set_nonblocking(int fd, int nb) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

static void set_timeouts(int fd, int sec) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

int connect_to_origin(const char *host, const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) return -1;

    int fd = -1;

    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        set_timeouts(fd, 10);

        if (set_nonblocking(fd, 1) < 0) { close(fd); fd = -1; continue; }

        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            set_nonblocking(fd, 0);
            break;
        }

        if (errno != EINPROGRESS) { close(fd); fd = -1; continue; }

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        int prc = poll(&pfd, 1, 5000);
        if (prc <= 0) { close(fd); fd = -1; continue; }

        int err = 0;
        socklen_t errlen = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0 || err != 0) {
            close(fd); fd = -1; continue;
        }

        set_nonblocking(fd, 0);
        break;
    }

    freeaddrinfo(res);
    return fd;
}

int send_all(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n == 0) return -1;
        if (errno == EINTR) continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            int prc = poll(&pfd, 1, 5000);
            if (prc <= 0) return -1;
            continue;
        }
        return -1;
    }

    return 0;
}
