SHELL = bash
CC = clang # gcc
CFLAGS = -Wall -g -ggdb -pg -Ofast -march=native -Wpedantic

all: bin/predis-server bin/predis bin/parallel-test bin/hashtable-test bin/parallel-hashtable-test bin/random-string-test bin/linked_list_test

test: bin/set-clean-test bin/update-test

types/names.txt: tools/type_list_gen.sh
	$< > $@

dt_hash.c: types/names.txt
	gperf --includes --readonly-tables $< > $@

tools/cmds.c: cmds.h cmds.txt
	echo -e "%{\n$$(cat cmds.h)\n%}\n%%\n$$(cat cmds.txt)" | gperf --readonly-tables > $@

tools/command_parser_hashes.h: tools/cmds.c tools/command_parser_hashgen.c
	$(CC) $(CFLAGS) -o tmp/command_parser_hashgen tools/command_parser_hashgen.c && tmp/command_parser_hashgen > tools/command_parser_hashes.h

command_parser.c: tools/command_parser_hashes.h tools/cmds.c

lib/hashtable.%.h: lib/hashtable.h
	$(CC) -DHT_VAL_TYPE="$*" -E "$<" -o "$@"

lib/hashtable.%.c: lib/hashtable.c
	if [ -a "hashtable-shared/$*.h" ]; then \
		(echo "#include \"hashtable-shared/$*.h\"" && echo "#line 1 \"$(abspath $<)\"" && cat lib/hashtable.c) > "tmp/hashtable.$*.c" && $(CC) -I. -Ilib/ -DHT_VAL_TYPE="$*" -E "tmp/hashtable.$*.c" -o "$@"; \
	else \
		$(CC) -DHT_VAL_TYPE="$*" -E lib/hashtable.c -o "$@"; \
	fi

tmp/hashtable.%.o: #lib/hashtable.%.c
	make "lib/hashtable.$*.c" && \
	$(CC) $(CFLAGS) "lib/hashtable.$*.c" -c -o "$@"

types/%.c: types/%.h types/ll_boilerplate.h types/template.h types/type_ll.h

tmp/types.%.o: types/%.c
	$(CC) $(CFLAGS) $< -c -o $@

tmp/remake_types: types/*.c
	touch $@ && make -s `echo $^ | sed "s/types\/\([^\.]*\)\.c/tmp\/types.\1.o/g"`

PREDIS_PREREQS = predis.c tmp/remake_types tmp/hashtable.struct\ main_ele.o lib/hashtable.struct\ main_ele.h dt_hash.c
PREDIS_DEPS = predis.c tmp/types.*.o "tmp/hashtable.struct main_ele.o"

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

bin/parallel-hashtable-test: tests/parallel-hashtable-test.c tests/parallel-test-template.c lib/random_string.c tmp/hashtable.char*.o
	$(CC) -g $(CFLAGS) -o $@ $< -pthread lib/random_string.c tmp/hashtable.char\*.o

bin/random-string-test: tests/random-string-test.c lib/random_string.c
	$(CC) -g $(CFLAGS) -o $@ $^

bin/hashtable-test: tests/hashtable-test.c tmp/hashtable.int.o lib/random_string.c
	$(CC) -g $(CFLAGS) -o $@ $^

bin/linked_list_test: tests/linked_list_test.c lib/linked_list.c
	$(CC) -g $(CFLAGS) -o $@ $^ -pthread

.PHONY: clean htest ptest serve clean_hard do_nothing

do_nothing:

serve: bin/predis-server
	$<

ptest: bin/parallel-test
	$<

htest: bin/hashtable-test
	$<

clean:
	rm -f bin/* cmds.c types/names.txt dt_hash.c command_parser_hashes.h tmp/command_parser_hashgen lib/hashtable.*.{c,h} tools/cmds.c tools/command_parser_hashes.h tmp/types.*.o tmp/hashtable.*.o tmp/remake_types

clean_hard: clean
	rm -f perf.* gmon.out tmp/* vgcore.*
