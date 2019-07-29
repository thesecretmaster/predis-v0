CC = gcc
CFLAGS = -Wall -g -pthread -pg -Ofast
LIBS = -lm

all: test cli

test: test.c predis.c types/*
	$(CC) $(CFLAGS) $(LIBS) -o bin/test test.c predis.c types/int.c

cli: cli.c predis.c types/*
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c predis.c types/int.c

clean:
	rm -f *.o main
