CC = gcc
CFLAGS = -std=c11 -O2 -pthread -Wall -Wextra -Werror

SRC = main.c list.c counter.c swapper.c lock.c
OBJ = $(SRC:.c=.o)

TARGET = lab23

.PHONY: all clean run-mutex run-spin run-rw

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run-mutex: $(TARGET)
	./$(TARGET) 1000 mutex

run-spin: $(TARGET)
	./$(TARGET) 1000 spin

run-rw: $(TARGET)
	./$(TARGET) 1000 rw
