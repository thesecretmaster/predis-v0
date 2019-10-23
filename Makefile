CC = gcc
CFLAGS = -Wall -g -pg -Ofast
LIBS = -lm

all: dt_hash.c cmds.c test bin/predis bin/predis-server

test: bin/set-clean-test bin/update-test

types/names.txt: type_list_gen.rb
	ruby type_list_gen.rb > types/names.txt

dt_hash.c: types/names.txt
	gperf --readonly-tables --hash-function-name=dt_hash --lookup-function-name=dt_valid types/names.txt > dt_hash.c

cmds.c: cmds.txt
	gperf --readonly-tables cmds.txt > cmds.c

bin/set-clean-test: tests/set-clean-test.c predis.c types/*.c types/*.h lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o $@ tests/set-clean-test.c predis.c types/*.c lib/hashtable.c

bin/update-test: tests/update-test.c predis.c types/*.c types/*.h lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -pthread -o $@ tests/update-test.c predis.c types/*.c lib/hashtable.c

bin/predis: cli.c predis.c types/*.c types/*.h command_parser.c lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c predis.c types/*.c command_parser.c lib/hashtable.c -ledit

bin/predis-server: cmds.c server.c predis.c types/*.c types/*.h command_parser.c lib/hashtable.c lib/hashtable.h
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis-server server.c predis.c types/*.c command_parser.c lib/hashtable.c -pthread

bin/gen_test_file: gen_test_file.c
	$(CC) $(CFLAGS) $(LIBS) -o bin/gen_test_file gen_test_file.c

clean:
	rm -f bin/* cmds.c types/names.txt dt_hash.c
