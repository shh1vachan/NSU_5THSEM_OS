#define _GNU_SOURCE
#include "proxy.h"
#include "http.h"
#include "net.h"
#include "cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <strings.h>
#include <pthread.h>

#define REQ_BUF_SIZE 16384
#define IO_BUF_SIZE  8192

static ssize_t find_header_end(const char *buf, size_t len) {
    if (len < 4)
        return -1;
    for (size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n')
            return (ssize_t)(i + 4);
    }
    return -1;
}

static void send_simple_error(int client_fd, const char *status, const char *msg) {
    char resp[512];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.0 %s\r\n"
                     "Connection: close\r\n"
                     "Content-Type: text/plain\r\n"
                     "\r\n"
                     "%s\n",
                     status, msg);
    if (n > 0)
        send_all(client_fd, resp, (size_t)n);
}

static void cache_put_gateway_error(cache_entry_t *e, const char *status, const char *msg) {
    char resp[512];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.0 %s\r\n"
                     "Connection: close\r\n"
                     "Content-Type: text/plain\r\n"
                     "\r\n"
                     "%s\n",
                     status, msg);
    if (n > 0) {
        cache_entry_add_data(e, resp, (size_t)n);
        cache_entry_mark_ready(e);
    } else {
        cache_entry_mark_error(e);
    }
}

static void *loader_thread(void *arg) {
    cache_entry_t *e = (cache_entry_t *)arg;

    int origin_fd = connect_to_origin(e->origin_host, e->origin_port);
    if (origin_fd < 0) {
        cache_put_gateway_error(e, "502 Bad Gateway", "Failed to connect to origin");
        return NULL;
    }

    if (send_all(origin_fd, e->req, e->req_len) < 0) {
        close(origin_fd);
        cache_put_gateway_error(e, "502 Bad Gateway", "Failed to send request to origin");
        return NULL;
    }

    char buf[IO_BUF_SIZE];
    ssize_t n;
    int got_any = 0;

    while ((n = recv(origin_fd, buf, sizeof(buf), 0)) > 0) {
        got_any = 1;
        cache_entry_add_data(e, buf, (size_t)n);
    }

    close(origin_fd);

    if (n < 0) {
        cache_put_gateway_error(e, "504 Gateway Timeout", "Failed while reading from origin");
        return NULL;
    }

    if (!got_any) {
        cache_put_gateway_error(e, "502 Bad Gateway", "Origin closed connection without response");
        return NULL;
    }

    cache_entry_mark_ready(e);
    return NULL;
}

void *client_thread(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int client_fd = ctx->client_fd;
    free(ctx);

    char req_buf[REQ_BUF_SIZE + 1];
    size_t req_len = 0;

    for (;;) {
        if (req_len >= REQ_BUF_SIZE) {
            send_simple_error(client_fd, "400 Bad Request", "Request too large");
            close(client_fd);
            return NULL;
        }
        ssize_t n = recv(client_fd, req_buf + req_len, REQ_BUF_SIZE - req_len, 0);
        if (n <= 0) {
            if (n < 0)
                perror("[ERR] recv from client");
            close(client_fd);
            return NULL;
        }
        req_len += (size_t)n;
        if (find_header_end(req_buf, req_len) != -1)
            break;
    }

    req_buf[req_len] = '\0';

    char method[16];
    char url[2048];
    char version[16];
    method[0] = url[0] = version[0] = '\0';

    if (http_parse_request_line(req_buf, req_len,
                                method, sizeof(method),
                                url, sizeof(url),
                                version, sizeof(version)) != 0) {
        send_simple_error(client_fd, "400 Bad Request", "Bad request line");
        close(client_fd);
        return NULL;
    }

    fprintf(stderr, "[INFO] request: %s %s %s\n", method, url, version);

    if (strcasecmp(method, "GET") != 0) {
        send_simple_error(client_fd, "501 Not Implemented", "Only GET is supported");
        close(client_fd);
        return NULL;
    }

    char host[256];
    char port[16];
    if (http_parse_url(url, host, sizeof(host), port, sizeof(port)) != 0) {
        send_simple_error(client_fd, "400 Bad Request", "Bad URL");
        close(client_fd);
        return NULL;
    }

    const char *prefix = "http://";
    const char *p = url + strlen(prefix);
    const char *slash = strchr(p, '/');
    const char *path = slash ? slash : "/";

    char *origin_req = NULL;
    size_t origin_req_len = 0;
    if (http_build_origin_request(req_buf, req_len,
                                  method, path, host,
                                  &origin_req, &origin_req_len) != 0) {
        send_simple_error(client_fd, "500 Internal Server Error", "Failed to build origin request");
        close(client_fd);
        return NULL;
    }

    fprintf(stderr, "[INFO] origin: %s:%s\n", host, port);

    int is_new = 0;
    cache_entry_t *entry = cache_get_or_create(url, host, port,
                                               origin_req, origin_req_len, &is_new);
    free(origin_req);
    if (!entry) {
        send_simple_error(client_fd, "500 Internal Server Error", "Cache failure");
        close(client_fd);
        return NULL;
    }

    if (is_new) {
        pthread_t tid;
        int rc = pthread_create(&tid, NULL, loader_thread, entry);
        if (rc != 0) {
            fprintf(stderr, "[ERR] pthread_create(loader): %s\n", strerror(rc));
            cache_entry_mark_error(entry);
        } else {
            pthread_detach(tid);
        }
    }

    char buf[IO_BUF_SIZE];
    size_t offset = 0;
    int finished = 0;

    while (!finished) {
        ssize_t n = cache_entry_read(entry, &offset, buf, sizeof(buf), &finished);
        if (n < 0) {
            fprintf(stderr, "[ERR] cache_entry_read failed\n");
            break;
        }
        if (n == 0 && finished)
            break;
        if (n > 0) {
            if (send_all(client_fd, buf, (size_t)n) < 0) {
                perror("[ERR] send_all to client");
                break;
            }
        }
    }

    cache_entry_release(entry);
    close(client_fd);
    return NULL;
}