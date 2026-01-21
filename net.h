#ifndef NET_H
#define NET_H

#include <stddef.h>

int connect_to_origin(const char *host, const char *port);

int send_all(int fd, const void *buf, size_t len);

#endif
