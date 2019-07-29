CC = gcc
CFLAGS = -Wall -g -pg -Ofast
LIBS = -lm

all: test cli

test: test.c predis.c types/*
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/test test.c predis.c types/*.c

cli: cli.c predis.c types/*
	$(CC) $(CFLAGS) $(LIBS) -lreadline -o bin/predis cli.c predis.c types/*.c

clean:
	rm -f bin/*
