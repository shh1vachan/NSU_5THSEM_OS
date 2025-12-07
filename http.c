#define _GNU_SOURCE
#include "http.h"

#include <string.h>
#include <stdio.h>

int http_parse_request_line(char *buf, size_t buf_len,
                            char *method, size_t method_sz,
                            char *url, size_t url_sz,
                            char *version, size_t version_sz) {
    (void)buf_len;

    char *line_end = strstr(buf, "\r\n");
    if (!line_end) {
        fprintf(stderr, "[ERR] no CRLF in request line\n");
        return -1;
    }

    *line_end = '\0';
    int n = sscanf(buf, "%15s %2047s %15s", method, url, version);
    *line_end = '\r';

    if (n != 3) {
        fprintf(stderr, "[ERR] failed to parse request line\n");
        return -1;
    }

    if (strlen(method) >= method_sz ||
        strlen(url) >= url_sz ||
        strlen(version) >= version_sz) {
        fprintf(stderr, "[ERR] method/url/version too long\n");
        return -1;
    }

    return 0;
}

int http_parse_url(const char *url,
                   char *host, size_t host_sz,
                   char *port, size_t port_sz) {
    const char *prefix = "http://";
    size_t prefix_len = strlen(prefix);

    if (strncmp(url, prefix, prefix_len) != 0) {
        fprintf(stderr, "[ERR] only http:// URLs are supported\n");
        return -1;
    }

    const char *p = url + prefix_len;
    const char *slash = strchr(p, '/');
    const char *host_end = slash ? slash : (url + strlen(url));

    const char *colon = NULL;
    for (const char *q = p; q < host_end; ++q) {
        if (*q == ':') {
            colon = q;
            break;
        }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - p);
        size_t port_len = (size_t)(host_end - colon - 1);

        if (host_len == 0 || host_len >= host_sz ||
            port_len == 0 || port_len >= port_sz) {
            fprintf(stderr, "[ERR] host or port too long in URL\n");
            return -1;
        }

        memcpy(host, p, host_len);
        host[host_len] = '\0';
        memcpy(port, colon + 1, port_len);
        port[port_len] = '\0';
    } else {
        size_t host_len = (size_t)(host_end - p);
        if (host_len == 0 || host_len >= host_sz) {
            fprintf(stderr, "[ERR] host too long in URL\n");
            return -1;
        }
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        snprintf(port, port_sz, "80");
    }

    return 0;
}
