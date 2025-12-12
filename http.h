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

int http_build_origin_request(const char *orig_req, size_t orig_len,
                              const char *method,
                              const char *path,
                              const char *host,
                              char **out_req, size_t *out_len);

#endif
