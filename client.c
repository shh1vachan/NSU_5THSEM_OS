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
#include <pthread.h>

#define REQ_BUF_SIZE 65536
#define IO_BUF_SIZE 65536

static const char *find_header_end(const char *buf, size_t len) {
    if (len < 4) return NULL;
    const char *p = memmem(buf, len, "\r\n\r\n", 4);
    return p ? (p + 4) : NULL;
}

static int recv_request_headers(int fd, char *buf, size_t cap, size_t *out_len) {
    size_t len = 0;
    while (len + 1 < cap) {
        ssize_t n = recv(fd, buf + len, cap - 1 - len, 0);
        if (n > 0) {
            len += (size_t)n;
            buf[len] = '\0';
            if (find_header_end(buf, len)) {
                if (out_len) *out_len = len;
                return 0;
            }
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        return -1;
    }
    return -1;
}

static int build_params_from_request(const char *req, size_t req_len,
                                     const char *url,
                                     char *host, size_t host_sz,
                                     char *port, size_t port_sz,
                                     const char **out_path) {
    if (strncmp(url, "http://", 7) == 0) {
        return http_parse_url(url, host, host_sz, port, port_sz, out_path);
    }

    if (url[0] == '/') {
        if (http_extract_host_header(req, req_len, host, host_sz, port, port_sz) < 0) return -1;
        *out_path = url;
        return 0;
    }

    return -1;
}

static int proxy_passthrough(int client_fd,
                             const char *origin_host,
                             const char *origin_port,
                             const char *origin_req,
                             size_t origin_req_len) {
    int origin_fd = connect_to_origin(origin_host, origin_port);
    if (origin_fd < 0) return -1;

    if (send_all(origin_fd, origin_req, origin_req_len) < 0) {
        close(origin_fd);
        return -1;
    }

    unsigned char buf[IO_BUF_SIZE];
    for (;;) {
        ssize_t n = recv(origin_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            if (send_all(client_fd, buf, (size_t)n) < 0) {
                close(origin_fd);
                return -1;
            }
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        close(origin_fd);
        return -1;
    }

    close(origin_fd);
    return 0;
}

static void write_simple_error(int client_fd, int code, const char *title) {
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.0 %d %s\r\n"
                        "Connection: close\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n",
                        code, title);
    if (hlen < 0) return;
    send_all(client_fd, hdr, (size_t)hlen);
}

static void write_gateway_error(cache_entry_t *e, int code, const char *title, const char *msg) {
    char body[512];
    int blen = snprintf(body, sizeof(body),
                        "<html><body><h1>%d %s</h1><p>%s</p></body></html>\n",
                        code, title, msg ? msg : "");
    if (blen < 0) blen = 0;
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.0 %d %s\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        code, title, blen);
    if (hlen < 0) hlen = 0;

    cache_entry_add_data(e, hdr, (size_t)hlen);
    if (blen > 0) cache_entry_add_data(e, body, (size_t)blen);
    cache_entry_mark_error(e);
}

static void *loader_thread(void *arg) {
    cache_entry_t *e = (cache_entry_t *)arg;

    int origin_fd = connect_to_origin(e->origin_host, e->origin_port);
    if (origin_fd < 0) {
        write_gateway_error(e, 502, "Bad Gateway", "Failed to connect to origin");
        cache_entry_release(e);
        return NULL;
    }

    if (send_all(origin_fd, e->req, e->req_len) < 0) {
        close(origin_fd);
        write_gateway_error(e, 502, "Bad Gateway", "Failed to send request to origin");
        cache_entry_release(e);
        return NULL;
    }

    unsigned char buf[IO_BUF_SIZE];
    for (;;) {
        ssize_t n = recv(origin_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            cache_entry_add_data(e, buf, (size_t)n);
            continue;
        }
        if (n == 0) {
            cache_entry_mark_ready(e);
            break;
        }
        if (errno == EINTR) continue;
        write_gateway_error(e, 504, "Gateway Timeout", "Failed to read from origin");
        break;
    }

    close(origin_fd);
    cache_entry_release(e);
    return NULL;
}

void *client_thread(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int client_fd = ctx->client_fd;
    free(ctx);

    char req_buf[REQ_BUF_SIZE];
    size_t req_len = 0;

    if (recv_request_headers(client_fd, req_buf, sizeof(req_buf), &req_len) < 0) {
        close(client_fd);
        return NULL;
    }

    char method[32], url[8192], version[32];
    if (http_parse_request_line(req_buf, req_len, method, sizeof(method), url, sizeof(url),
                                version, sizeof(version)) < 0) {
        close(client_fd);
        return NULL;
    }

    char host[1024], port[16];
    const char *path = NULL;

    if (build_params_from_request(req_buf, req_len, url, host, sizeof(host), port, sizeof(port), &path) < 0) {
        write_simple_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return NULL;
    }

    char *origin_req = NULL;
    size_t origin_req_len = 0;

    if (http_build_origin_request(req_buf, req_len, method, path, host, &origin_req, &origin_req_len) < 0) {
        write_simple_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return NULL;
    }

    if (strcasecmp(method, "GET") != 0) {
        (void)proxy_passthrough(client_fd, host, port, origin_req, origin_req_len);
        free(origin_req);
        close(client_fd);
        return NULL;
    }

    char cache_key[9216];
    if (http_build_cache_key(host, port, path, cache_key, sizeof(cache_key)) < 0) {
        free(origin_req);
        write_simple_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return NULL;
    }

    int is_new = 0;
    cache_entry_t *entry = cache_get_or_create(cache_key, host, port, origin_req, origin_req_len, &is_new);
    free(origin_req);

    if (!entry) {
        write_simple_error(client_fd, 503, "Service Unavailable");
        close(client_fd);
        return NULL;
    }

    if (is_new) {
        cache_entry_acquire(entry);
        pthread_t tid;
        if (pthread_create(&tid, NULL, loader_thread, entry) == 0) {
            pthread_detach(tid);
        } else {
            cache_entry_release(entry);
            write_gateway_error(entry, 503, "Service Unavailable", "Failed to start loader thread");
        }
    }

    unsigned char buf[IO_BUF_SIZE];
    size_t offset = 0;

    for (;;) {
        int finished = 0;
        ssize_t n = cache_entry_read(entry, &offset, buf, sizeof(buf), &finished);
        if (n < 0) break;
        if (n > 0) {
            if (send_all(client_fd, buf, (size_t)n) < 0) break;
        }
        if (finished && n == 0) break;
    }

    cache_entry_release(entry);
    close(client_fd);
    return NULL;
}
