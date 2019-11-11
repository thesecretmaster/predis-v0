#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../lib/random_string.h"
#define HT_VAL_TYPE int
#include "../lib/hashtable.h"


int main() {
  struct ht_table *table = ht_init(8);
  char *s;
  int *j;
  int f;
  srand((unsigned int) time(NULL));
  for (int i = 0; i < 300; i++) {
    // printf("+STORE\n");
    s = random_string('k');
    if ((f = ht_store(table, s, &i)) != 0) {
      printf("Yikes %d\n", f);
    }
    // printf("-STORE+FIND\n");
    j = ht_find(table, s);
    if (j == NULL || *j != i) {
      if (j == NULL) {
        printf("%d: err(null) %X %s\n", i, ht_hash(s), s);
      } else {
        printf("%d: err%d %X %s\n", i, *j, ht_hash(s), s);
      }
    }
    // printf("-FIND\n");
    free(s);
  }
  ht_free(table);
  return 0;
}
