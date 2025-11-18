CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: main

libmythread.a: mythread.o
	ar rcs libmythread.a mythread.o

mythread.o: mythread.c mythread_lib.h
	$(CC) $(CFLAGS) -c mythread.c

main: main.o libmythread.a
	$(CC) $(CFLAGS) -o main main.o -L. -lmythread -lpthread

main.o: main.c mythread_lib.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o main libmythread.a
