#include <stdio.h>
#include "lib/hashtable.h"

int main() {
  struct ht_table *table = ht_init(100);
  ht_store(table, "foobar", 123);
  int res = ht_find(table, "foobar");
  ht_store(table, "flobar", 321);
  res = ht_find(table, "foobar");
  printf("Hi %d\n", res);
  return 0;
}
