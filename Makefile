CC = gcc
CFLAGS = -Wall -g -pg -Ofast
LIBS = -lm

all: test bin/predis bin/predis-server

test: bin/set-clean-test bin/update-test

cmds.c: cmds.txt
	gperf --readonly-tables cmds.txt > cmds.c

bin/set-clean-test: set-clean-test.c predis.c types/*.c types/*.h lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/set-clean-test set-clean-test.c predis.c types/*.c lib/hashtable.c

bin/update-test: update-test.c predis.c types/*.c types/*.h lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o bin/update-test update-test.c predis.c types/*.c lib/hashtable.c

bin/predis: cli.c predis.c types/*.c types/*.h command_parser.c lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c predis.c types/*.c command_parser.c lib/hashtable.c -ledit

bin/predis-server: cmds.c server.c predis.c types/*.c types/*.h command_parser.c lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis-server server.c predis.c types/*.c command_parser.c lib/hashtable.c -pthread

bin/gen_test_file: gen_test_file.c
	$(CC) $(CFLAGS) $(LIBS) -o bin/gen_test_file gen_test_file.c

clean:
	rm -f bin/* cmds.c
