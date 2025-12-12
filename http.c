#define _GNU_SOURCE
#include "http.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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

static int ieq_prefix(const char *line, const char *prefix) {
    size_t n = strlen(prefix);
    for (size_t i = 0; i < n; ++i) {
        if (tolower((unsigned char)line[i]) != tolower((unsigned char)prefix[i]))
            return 0;
    }
    return 1;
}

int http_build_origin_request(const char *orig_req, size_t orig_len,
                              const char *method,
                              const char *path,
                              const char *host,
                              char **out_req, size_t *out_len) {
    const char *p = orig_req;
    const char *end = orig_req + orig_len;

    const char *line_end = strstr(p, "\r\n");
    if (!line_end) {
        fprintf(stderr, "[ERR] no CRLF in request\n");
        return -1;
    }
    const char *headers_start = line_end + 2;

    const char *headers_end = NULL;
    for (const char *q = headers_start; q + 3 <= end; ++q) {
        if (q[0] == '\r' && q[1] == '\n' && q[2] == '\r' && q[3] == '\n') {
            headers_end = q + 4;
            break;
        }
    }
    if (!headers_end) {
        fprintf(stderr, "[ERR] no header end in request\n");
        return -1;
    }

    size_t out_cap = orig_len + 256;
    char *out = malloc(out_cap);
    if (!out)
        return -1;
    size_t out_pos = 0;

    int n = snprintf(out + out_pos, out_cap - out_pos,
                     "%s %s HTTP/1.0\r\n", method, path);
    if (n <= 0 || (size_t)n >= out_cap - out_pos) {
        free(out);
        return -1;
    }
    out_pos += (size_t)n;

    const char *h = headers_start;
    while (h < headers_end - 2) {
        const char *le = strstr(h, "\r\n");
        if (!le || le > headers_end - 2)
            break;
        size_t line_len = (size_t)(le - h);
        if (line_len == 0) {
            h = le + 2;
            continue;
        }

        int skip = 0;
        if (line_len >= 5 && ieq_prefix(h, "Host:"))
            skip = 1;
        else if (line_len >= 11 && ieq_prefix(h, "Connection:"))
            skip = 1;
        else if (line_len >= 17 && ieq_prefix(h, "Proxy-Connection:"))
            skip = 1;

        if (!skip) {
            if (out_pos + line_len + 2 > out_cap) {
                size_t new_cap = out_cap * 2;
                while (new_cap < out_pos + line_len + 2)
                    new_cap *= 2;
                char *tmp = realloc(out, new_cap);
                if (!tmp) {
                    free(out);
                    return -1;
                }
                out = tmp;
                out_cap = new_cap;
            }
            memcpy(out + out_pos, h, line_len);
            out_pos += line_len;
            out[out_pos++] = '\r';
            out[out_pos++] = '\n';
        }

        h = le + 2;
    }

    n = snprintf(out + out_pos, out_cap - out_pos,
                 "Host: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 host);
    if (n <= 0 || (size_t)n >= out_cap - out_pos) {
        free(out);
        return -1;
    }
    out_pos += (size_t)n;

    *out_req = out;
    *out_len = out_pos;
    return 0;
}
