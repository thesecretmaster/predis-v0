#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

char *random_string(char prefix) {
  int length = (rand() % 40) + 5;
  char *str = malloc(sizeof(char)*length + 1);
  str[length] = '\0';
  str[0] = prefix;
  for (int i = 1; i < length; i++) {
    str[i] = charset[rand() % (sizeof(charset) - 1)];
  }
  return str;
}

struct item {
  char *key;
  char *value;
  bool set;
  bool get;
};

#define ITEM_COUNT 10000

int main() {
  struct item items[ITEM_COUNT];
  for (int i = 0; i < ITEM_COUNT; i++) {
    items[i].key = random_string('k');
    items[i].value = random_string('v');
    items[i].set = false;
    items[i].get = false;
  }
  int count = 0;
  struct item *item;
  int idx;
  FILE *f = fopen("testfile", "w");
  FILE *f1 = fopen("testfile.expout", "w");
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
