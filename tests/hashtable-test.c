#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include "../lib/random_string.h"
#define HT_VAL_TYPE int
#include "../lib/hashtable.h"

int initEle(int *v) {
  *v = 0;
  return 0;
}

int main(int argc, char *argv[]) {
  int c;
  int ht_count = 10000;

  while ((c = getopt (argc, argv, "c:")) != -1)
   switch (c)
     {
     case 'c':
       ht_count = strtol(optarg, NULL, 10);
       break;
     }

  printf("Running test with %d entries\n", ht_count);
  struct ht_table *table = ht_init(8, &initEle);
  char *s;
  int *j;
  int f;
  srand((unsigned int) time(NULL));
  for (int i = 0; i < ht_count; i++) {
    // printf("+STORE\n");
    s = random_string('k', NULL, NULL);
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
