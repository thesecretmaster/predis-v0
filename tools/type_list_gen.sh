#!/bin/bash
echo "%{"
echo "static __inline unsigned int hash(const char*, unsigned int);"
echo "unsigned int dt_hash(const char *a, unsigned int b) { return hash(a,b);}"
echo "static __inline const char * in_word_set (const char *, unsigned int);"
echo "const char * dt_valid(const char *a, unsigned int b) { return in_word_set(a,b);}"
echo "%}"
echo "%%"
for ent in types/*
  do echo $ent
done | sed "s/types\/\([^\.]*\).*/\1/" | uniq -c | grep "\s*2\s" | sed "s/[[:space:]]*[0-9]*[[:space:]]\(.*\)/\1/g"
