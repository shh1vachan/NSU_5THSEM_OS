CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -pthread
LDFLAGS = -pthread

OBJS = main.o client.o http.o net.o

all: proxy

proxy: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c proxy.h
client.o: client.c proxy.h http.h net.h
http.o: http.c http.h
net.o: net.c net.h

clean:
	rm -f $(OBJS) proxy
