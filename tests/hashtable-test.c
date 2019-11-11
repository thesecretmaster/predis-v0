#include <stdio.h>
#include <stdlib.h>
#include "../lib/random_string.h"
#define HT_VAL_TYPE int
#include "../lib/hashtable.h"


int main() {
  struct ht_table *table = ht_init(8);
  char *s;
  int *j;
  int f;
  for (int i = 0; i < 100; i++) {
    s = random_string('k');
    if ((f = ht_store(table, s, &i)) != 0) {
      printf("Yikes %d\n", f);
    }
    j = ht_find(table, s);
    if (j == NULL || *j != i) {
      printf("%d: err %X %d %s\n", i, ht_hash(s), rand(), s);
    }
    free(s);
  }
  ht_free(table);
  return 0;
}
