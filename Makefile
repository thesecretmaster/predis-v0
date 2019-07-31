CC = gcc
CFLAGS = -Wall -g -pg -Ofast
LIBS = -lm

all: bin/test bin/predis

bin/test: test.c predis.c types/*.c types/*.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/test test.c predis.c types/*.c

bin/predis: cli.c predis.c types/*.c types/*.h
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c predis.c types/*.c -ledit

clean:
	rm -f bin/*
