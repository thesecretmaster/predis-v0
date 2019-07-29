CC = gcc
CFLAGS = -Wall -g -pthread
LIBS = -lm

all: main

main: main.c lib.c types/*
	$(CC) $(CFLAGS) $(LIBS) -o main main.c types/int.c

clean:
	rm -f *.o main
