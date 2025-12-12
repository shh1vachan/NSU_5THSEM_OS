CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -pthread
LDFLAGS = -pthread

OBJS = main.o client.o http.o net.o cache.o

all: proxy

proxy: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c proxy.h cache.h
client.o: client.c proxy.h http.h net.h cache.h
http.o: http.c http.h
net.o: net.c net.h
cache.o: cache.c cache.h

clean:
	rm -f $(OBJS) proxy
