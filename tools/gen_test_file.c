#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "../lib/random_string.h"

struct item {
  char *key;
  char *value;
  bool set;
  bool get;
};

#define ITEM_COUNT 100000

const char suffix[] = ".expout";
const char default_filename[] = "tmp/testfile";

int main(int argc, char *argv[]) {
  struct item items[ITEM_COUNT];
  for (int i = 0; i < ITEM_COUNT; i++) {
    items[i].key = random_string('k', NULL, NULL);
    items[i].value = random_string('v', NULL, NULL);
    items[i].set = false;
    items[i].get = false;
  }
  int count = 0;
  struct item *item;
  int idx;
  const char *filename = argc <= 1 ? default_filename : argv[1];
  char *filename_final = malloc(sizeof(char) * (strlen(filename) + sizeof(suffix)));
  strcpy(filename_final, filename);
  strcat(filename_final, ".expout");
  printf("%s\n%s\n", filename, filename_final);
  FILE *f = fopen(filename, "w");
  FILE *f1 = fopen(filename_final, "w");
  if (f == NULL || f1 == NULL) { return -1; }
  while (count < ITEM_COUNT*2) {
    idx = rand() % ITEM_COUNT;
    item = items + idx;
    if (item->set == false) {
      fprintf(f, "set %s %s\n", item->key, item->value);
      fprintf(f1, "OK\n");
      item->set = true;
      count++;
    } else if (item->get == false) {
      fprintf(f, "get %s\n", item->key);
      fprintf(f1, "%s\n", item->value);
      item->get = true;
      count++;
    }
  }
}
