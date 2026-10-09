CC = gcc
CFLAGS = -Wall -Wextra

SRC = src/ls-v1.0.0.c

all: bin/ls

bin/ls: $(SRC)
	$(CC) $(CFLAGS) -o bin/ls $(SRC)

clean:
	rm -f bin/ls obj/*.o

.PHONY: all clean
