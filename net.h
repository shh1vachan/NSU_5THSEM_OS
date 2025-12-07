#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <sys/types.h>

ssize_t send_all(int fd, const void *buf, size_t len);
int connect_to_origin(const char *host, const char *port);

#endif
