CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: main

libuthread.a: uthread.o
	ar rcs libuthread.a uthread.o

uthread.o: uthread.c uthread_lib.h
	$(CC) $(CFLAGS) -c uthread.c

main.o: main.c uthread_lib.h
	$(CC) $(CFLAGS) -c main.c

main: main.o libuthread.a
	$(CC) $(CFLAGS) -o main main.o -L. -luthread

clean:
	rm -f *.o main libuthread.a
