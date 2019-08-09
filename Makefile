CC = gcc
CFLAGS = -Wall -g -pg -Ofast
LIBS = -lm

all: test bin/predis bin/predis-server

test: bin/set-clean-test bin/update-test

bin/set-clean-test: set-clean-test.c predis.c types/*.c types/*.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/set-clean-test set-clean-test.c predis.c types/*.c

bin/update-test: update-test.c predis.c types/*.c types/*.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/update-test update-test.c predis.c types/*.c

bin/predis: cli.c predis.c types/*.c types/*.h command_parser.c
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c predis.c types/*.c command_parser.c -ledit

bin/predis-server: server.c predis.c types/*.c types/*.h command_parser.c
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis-server server.c predis.c types/*.c command_parser.c -pthread

clean:
	rm -f bin/*
