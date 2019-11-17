#include "../lib/random_string.h"
#include <stdio.h>
int main() {
  unsigned int *i;
  int len;
  char *key = random_string('k', &i, &len);
  printf("key: %s\n", key);
  for (int j = 0; j < len; j++) {
    printf("int %d: %X\n", j, i[j]);
  }
  return 0;
}
