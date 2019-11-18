SHELL = bash
CC = gcc
CFLAGS = -Wall -g -ggdb -pg -Ofast

all: bin/predis-server bin/predis bin/parallel-test bin/hashtable-test bin/parallel-hashtable-test bin/random-string-test

test: bin/set-clean-test bin/update-test

types/names.txt: tools/type_list_gen.sh
	$< > $@

dt_hash.c: types/names.txt
	gperf --includes --readonly-tables --hash-function-name=dt_hash --lookup-function-name=dt_valid $< > $@

tools/cmds.c: cmds.txt
	gperf --readonly-tables $< > $@

tools/command_parser_hashes.h: tools/cmds.c tools/command_parser_hashgen.c
	gcc $(CFLAGS) -o tmp/command_parser_hashgen tools/command_parser_hashgen.c && tmp/command_parser_hashgen > tools/command_parser_hashes.h

command_parser.c: tools/command_parser_hashes.h tools/cmds.c

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
	$(CC) $(CFLAGS) -pthread -o $@ $< $(PREDIS_DEPS)

bin/update-test: tests/update-test.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) -pthread -o $@ $< $(PREDIS_DEPS)

bin/predis: interfaces/cli.c command_parser.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) -o bin/predis $< command_parser.c $(PREDIS_DEPS) -ledit

bin/predis-server: interfaces/server.c command_parser.c $(PREDIS_PREREQS)
	$(CC) $(CFLAGS) -o $@ $< command_parser.c $(PREDIS_DEPS) -pthread

bin/gen_test_file: tools/gen_test_file.c lib/random_string.c
	$(CC) $(CFLAGS) -o $@ $^

bin/parallel-test: tests/parallel-test.c tests/parallel-test-template.c lib/random_string.c
	$(CC) -g $(CFLAGS) -o $@ $< -lhiredis -lpthread lib/random_string.c

bin/parallel-hashtable-test: tests/parallel-hashtable-test.c tests/parallel-test-template.c lib/random_string.c lib/hashtable.char\ *.c
	$(CC) -g $(CFLAGS) -o $@ $< -pthread lib/random_string.c lib/hashtable.char\ \*.c

bin/random-string-test: tests/random-string-test.c lib/random_string.c
	$(CC) -g $(CFLAGS) -o $@ $^

bin/hashtable-test: tests/hashtable-test.c lib/hashtable.int.c lib/random_string.c
	$(CC) -g $(CFLAGS) -o $@ $^

.PHONY: clean htest ptest serve clean_hard

serve: bin/predis-server
	$<

ptest: bin/parallel-test
	$<

htest: bin/hashtable-test
	$<

clean:
	rm -f bin/* cmds.c types/names.txt dt_hash.c command_parser_hashes.h tmp/command_parser_hashgen lib/hashtable.*.{c,h} tools/cmds.c tools/command_parser_hashes.h

clean_hard: clean
	rm -f perf.* gmon.out tmp/* vgcore.*
