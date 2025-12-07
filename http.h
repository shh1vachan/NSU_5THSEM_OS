#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

int http_parse_request_line(char *buf, size_t buf_len,
                            char *method, size_t method_sz,
                            char *url, size_t url_sz,
                            char *version, size_t version_sz);

int http_parse_url(const char *url,
                   char *host, size_t host_sz,
                   char *port, size_t port_sz);

#endif
