CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

all: lab

lab: main.o lock.o
	$(CC) $(CFLAGS) -o $@ main.o lock.o

main.o: main.c lock.h
	$(CC) $(CFLAGS) -c main.c

lock.o: lock.c lock.h
	$(CC) $(CFLAGS) -c lock.c

clean:
	rm -f *.o lab
