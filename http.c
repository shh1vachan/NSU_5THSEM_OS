#define _GNU_SOURCE
#include "http.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int http_parse_request_line(char *buf, size_t buf_len,
                            char *method, size_t method_sz,
                            char *url, size_t url_sz,
                            char *version, size_t version_sz) {
    if (!buf || buf_len == 0) return -1;

    char *line_end = memmem(buf, buf_len, "\r\n", 2);
    if (!line_end) return -1;

    *line_end = '\0';

    char *sp1 = strchr(buf, ' ');
    if (!sp1) return -1;
    *sp1++ = '\0';
    while (*sp1 == ' ') sp1++;

    char *sp2 = strchr(sp1, ' ');
    if (!sp2) return -1;
    *sp2++ = '\0';
    while (*sp2 == ' ') sp2++;

    if (snprintf(method, method_sz, "%s", buf) >= (int)method_sz) return -1;
    if (snprintf(url, url_sz, "%s", sp1) >= (int)url_sz) return -1;
    if (snprintf(version, version_sz, "%s", sp2) >= (int)version_sz) return -1;

    line_end[0] = '\r';
    line_end[1] = '\n';

    return 0;
}

int http_parse_url(const char *url,
                   char *host, size_t host_sz,
                   char *port, size_t port_sz,
                   const char **out_path) {
    if (!url || !host || !port) return -1;

    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else return -1;

    const char *host_start = p;
    while (*p && *p != '/' && *p != ':') p++;
    const char *host_end = p;

    if (host_end == host_start) return -1;

    size_t hlen = (size_t)(host_end - host_start);
    if (hlen + 1 > host_sz) return -1;

    memcpy(host, host_start, hlen);
    host[hlen] = '\0';

    const char *path = "/";
    if (*p == ':') {
        p++;
        const char *port_start = p;
        while (*p && *p != '/') p++;
        size_t plen = (size_t)(p - port_start);
        if (plen == 0 || plen + 1 > port_sz) return -1;
        memcpy(port, port_start, plen);
        port[plen] = '\0';
        if (*p == '/') path = p;
    } else {
        snprintf(port, port_sz, "80");
        if (*p == '/') path = p;
    }

    if (out_path) *out_path = path;
    return 0;
}

int http_extract_host_header(const char *req, size_t req_len,
                             char *host, size_t host_sz,
                             char *port, size_t port_sz) {
    if (!req || !host || !port) return -1;

    size_t i = 0;
    while (i + 2 < req_len) {
        const char *line = req + i;
        const char *end = memmem(line, req_len - i, "\r\n", 2);
        if (!end) break;
        size_t linelen = (size_t)(end - line);
        i += linelen + 2;
        if (linelen == 0) break;

        if (linelen >= 5 && strncasecmp(line, "Host:", 5) == 0) {
            const char *v = line + 5;
            while (*v == ' ' || *v == '\t') v++;
            const char *vend = line + linelen;
            while (vend > v && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;

            const char *colon = NULL;
            for (const char *q = v; q < vend; q++) {
                if (*q == ':') { colon = q; break; }
            }

            if (!colon) {
                size_t hlen = (size_t)(vend - v);
                if (hlen + 1 > host_sz) return -1;
                memcpy(host, v, hlen);
                host[hlen] = '\0';
                snprintf(port, port_sz, "80");
                return 0;
            } else {
                size_t hlen = (size_t)(colon - v);
                size_t plen = (size_t)(vend - (colon + 1));
                if (hlen + 1 > host_sz || plen == 0 || plen + 1 > port_sz) return -1;
                memcpy(host, v, hlen);
                host[hlen] = '\0';
                memcpy(port, colon + 1, plen);
                port[plen] = '\0';
                return 0;
            }
        }
    }

    return -1;
}

static int header_name_eq(const char *line, size_t linelen, const char *name) {
    size_t nlen = strlen(name);
    if (linelen < nlen + 1) return 0;
    if (strncasecmp(line, name, nlen) != 0) return 0;
    return line[nlen] == ':';
}

int http_build_origin_request(const char *orig_req, size_t orig_len,
                              const char *method,
                              const char *path,
                              const char *host,
                              char **out_req, size_t *out_len) {
    if (!orig_req || !method || !path || !host || !out_req || !out_len) return -1;

    const char *hdr_end = memmem(orig_req, orig_len, "\r\n\r\n", 4);
    if (!hdr_end) return -1;
    size_t header_block_len = (size_t)(hdr_end - orig_req) + 4;

    size_t cap = header_block_len + 256;
    char *buf = (char *)malloc(cap);
    if (!buf) return -1;

    int w = snprintf(buf, cap, "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n",
                     method, path, host);
    if (w < 0 || (size_t)w >= cap) { free(buf); return -1; }
    size_t pos = (size_t)w;

    size_t i = 0;
    const char *first_end = memmem(orig_req, orig_len, "\r\n", 2);
    if (!first_end) { free(buf); return -1; }
    i = (size_t)(first_end - orig_req) + 2;

    while (i < header_block_len) {
        const char *line = orig_req + i;
        const char *end = memmem(line, header_block_len - i, "\r\n", 2);
        if (!end) break;
        size_t linelen = (size_t)(end - line);
        i += linelen + 2;
        if (linelen == 0) break;

        if (header_name_eq(line, linelen, "Host")) continue;
        if (header_name_eq(line, linelen, "Connection")) continue;
        if (header_name_eq(line, linelen, "Proxy-Connection")) continue;
        if (header_name_eq(line, linelen, "Keep-Alive")) continue;
        if (header_name_eq(line, linelen, "TE")) continue;
        if (header_name_eq(line, linelen, "Trailer")) continue;
        if (header_name_eq(line, linelen, "Transfer-Encoding")) continue;
        if (header_name_eq(line, linelen, "Upgrade")) continue;

        if (pos + linelen + 2 + 2 >= cap) {
            cap = (cap * 2) + linelen + 128;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); return -1; }
            buf = nb;
        }
        memcpy(buf + pos, line, linelen);
        pos += linelen;
        memcpy(buf + pos, "\r\n", 2);
        pos += 2;
    }

    if (pos + 2 >= cap) {
        char *nb = (char *)realloc(buf, cap + 8);
        if (!nb) { free(buf); return -1; }
        buf = nb;
        cap += 8;
    }
    memcpy(buf + pos, "\r\n", 2);
    pos += 2;

    *out_req = buf;
    *out_len = pos;
    return 0;
}

int http_build_cache_key(const char *host, const char *port, const char *path,
                         char *out, size_t out_sz) {
    if (!host || !port || !path || !out || out_sz == 0) return -1;

    size_t hlen = strlen(host);
    size_t plen = strlen(port);
    size_t pathlen = strlen(path);

    if (7 + hlen + 1 + plen + pathlen + 1 > out_sz) return -1;

    char *p = out;
    memcpy(p, "http://", 7);
    p += 7;

    for (size_t i = 0; i < hlen; i++) *p++ = (char)tolower((unsigned char)host[i]);

    *p++ = ':';
    memcpy(p, port, plen);
    p += plen;

    if (path[0] != '/') *p++ = '/';
    memcpy(p, path, pathlen);
    p += pathlen;

    *p = '\0';
    return 0;
}
