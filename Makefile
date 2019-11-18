CC = gcc
CFLAGS = -Wall -g -ggdb -pg -Ofast
LIBS = -lm

all: dt_hash.c cmds.c bin/predis-server bin/predis bin/parallel-test bin/hashtable-test bin/parallel-hashtable-test bin/random-string-test

test: bin/set-clean-test bin/update-test

types/names.txt: type_list_gen.sh
	./type_list_gen.sh > types/names.txt

dt_hash.c: types/names.txt
	gperf --includes --readonly-tables --hash-function-name=dt_hash --lookup-function-name=dt_valid types/names.txt > dt_hash.c

cmds.c: cmds.txt
	gperf --readonly-tables cmds.txt > cmds.c

command_parser_hashgen: cmds.c command_parser_hashgen.c
	gcc $(CFLAGS) -o $@ command_parser_hashgen.c

command_parser_hashes.h: command_parser_hashgen
	./command_parser_hashgen > command_parser_hashes.h

command_parser.c: command_parser_hashes.h

lib/hashtable.%.h: lib/hashtable.h
	gcc -DHT_VAL_TYPE="$*" -E "$<" -o "$@"

lib/hashtable.%.c: lib/hashtable.c
	if [ -a "hashtable-shared/$*.h" ]; then \
		(echo "#include \"hashtable-shared/$*.h\"" && cat lib/hashtable.c) > "tmp/hashtable.$*.c" && gcc -I. -Ilib/ -DHT_VAL_TYPE="$*" -E "tmp/hashtable.$*.c" -o "$@"; \
	else \
		gcc -DHT_VAL_TYPE="$*" -E lib/hashtable.c -o "$@"; \
	fi

PREDIS_PREREQS = predis.c types/*.c lib/hashtable.struct\ main_ele.c lib/hashtable.struct\ main_ele.h dt_hash.c
PREDIS_DEPS = predis.c types/*.c "lib/hashtable.struct main_ele.c"

bin/set-clean-test: tests/set-clean-test.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) $(LIBS) -pthread -o $@ tests/set-clean-test.c $(PREDIS_DEPS)

bin/update-test: tests/update-test.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) $(LIBS) -pthread -o $@ tests/update-test.c $(PREDIS_DEPS)

bin/predis: cli.c command_parser.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) $(LIBS) -o bin/predis cli.c $(PREDIS_DEPS) command_parser.c -ledit

bin/predis-server: server.c cmds.c command_parser.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $< -pthread $(PREDIS_DEPS) command_parser.c

serve: bin/predis-server
	$<

ptest: bin/parallel-test
	$<

htest: bin/hashtable-test
	$<

bin/gen_test_file: gen_test_file.c
	$(CC) $(CFLAGS) $(LIBS) -o bin/gen_test_file gen_test_file.c

bin/parallel-test: tests/parallel-test.c tests/parallel-test-template.c lib/random_string.c
	gcc -g $(CFLAGS) -o $@ $< -lhiredis -lpthread lib/random_string.c

bin/parallel-hashtable-test: tests/parallel-hashtable-test.c tests/parallel-test-template.c lib/random_string.c lib/hashtable.char\ *.c
	gcc -g $(CFLAGS) -o $@ $< -lpthread lib/random_string.c lib/hashtable.char\ \*.c

bin/random-string-test: tests/random-string-test.c lib/random_string.c
	gcc -g $(CFLAGS) -o $@ $< lib/random_string.c

bin/hashtable-test: tests/hashtable-test.c lib/hashtable.int.c lib/random_string.c
	gcc -g $(CFLAGS) -o $@ $< lib/hashtable.int.c lib/random_string.c

clean:
	rm -f bin/* cmds.c types/names.txt dt_hash.c command_parser_hashes.h command_parser_hashgen lib/hashtable.*.{c,h}
