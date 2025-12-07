#define _GNU_SOURCE
#include "proxy.h"
#include "http.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <strings.h>

#define REQ_BUF_SIZE 8192
#define IO_BUF_SIZE  8192

void *client_thread(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int client_fd = ctx->client_fd;
    free(ctx);

    char req_buf[REQ_BUF_SIZE + 1];
    ssize_t req_len = recv(client_fd, req_buf, REQ_BUF_SIZE, 0);
    if (req_len <= 0) {
        if (req_len < 0) {
            perror("[ERR] recv from client");
        }
        close(client_fd);
        return NULL;
    }

    size_t zlen = (size_t)req_len;
    if (zlen > REQ_BUF_SIZE)
        zlen = REQ_BUF_SIZE;
    req_buf[zlen] = '\0';

    char method[16];
    char url[2048];
    char version[16];
    method[0] = url[0] = version[0] = '\0';

    if (http_parse_request_line(req_buf, zlen,
                                method, sizeof(method),
                                url, sizeof(url),
                                version, sizeof(version)) != 0) {
        const char *resp =
            "HTTP/1.0 400 Bad Request\r\n"
            "Connection: close\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Bad request line.\n";
        send_all(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    fprintf(stderr, "[INFO] request: %s %s %s\n", method, url, version);

    if (strcasecmp(method, "GET") != 0) {
        const char *resp =
            "HTTP/1.0 501 Not Implemented\r\n"
            "Connection: close\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Only GET is supported.\n";
        send_all(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    char host[256];
    char port[16];
    if (http_parse_url(url, host, sizeof(host), port, sizeof(port)) != 0) {
        const char *resp =
            "HTTP/1.0 400 Bad Request\r\n"
            "Connection: close\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Bad URL.\n";
        send_all(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    fprintf(stderr, "[INFO] origin: %s:%s\n", host, port);

    int origin_fd = connect_to_origin(host, port);
    if (origin_fd < 0) {
        const char *resp =
            "HTTP/1.0 502 Bad Gateway\r\n"
            "Connection: close\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Failed to connect to origin.\n";
        send_all(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    if (send_all(origin_fd, req_buf, (size_t)req_len) < 0) {
        perror("[ERR] send_all to origin");
        close(origin_fd);
        close(client_fd);
        return NULL;
    }

    char buf[IO_BUF_SIZE];
    ssize_t n;

    while ((n = recv(origin_fd, buf, sizeof(buf), 0)) > 0) {
        if (send_all(client_fd, buf, (size_t)n) < 0) {
            perror("[ERR] send_all to client");
            break;
        }
    }

    if (n < 0) {
        perror("[ERR] recv from origin");
    }

    close(origin_fd);
    close(client_fd);
    return NULL;
}
