#include <stdlib.h>
#include <limits.h>
#include "random_string.h"

static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static unsigned int *hash_randstr(unsigned int*, int, int*);

char *random_string(char prefix, unsigned int **hsh, int *khshlen) {
  int length = (rand() % 40) + 5;
  char *str = malloc(sizeof(char)*(length + 1));
  str[length] = '\0';
  str[0] = prefix;
  unsigned int idxs[length-1];
  for (int i = 1; i < length; i++) {
    idxs[i-1] = rand() % (sizeof(charset) - 1);
    str[i] = charset[idxs[i-1]];
  }
  if (hsh != NULL && sizeof(charset) < 256 /* 2^8 (8 bits long) */ && sizeof(unsigned int) == 4) {
    *hsh = hash_randstr(idxs, length, khshlen);
  }
  return str;
}

static unsigned int *hash_randstr(unsigned int *idxs, int length, int *mutlen) {
  // (length / 4) round up + 1 (0x0)
  int len_div_4 = ((length / 4) + (length % 4 == 0 ? 0 : 1));
  if (mutlen != NULL) {
    *mutlen = len_div_4;
  }
  unsigned int *hsh = malloc(sizeof(unsigned int)*(len_div_4 + 1));
  for (int i = 0; i < len_div_4 + 1; i++) {
    hsh[i] = 0x0;
  }
  // printf("Allocating hash size %d (str: %d)\n", len_div_4, length);
  int incr = 0;
  int i;
  for (i = 0; i < length - 2; i++) {
    // printf("Touching idx %lu(+%d), byte %lu (%X)\n", i / sizeof(unsigned int), incr, (i / sizeof(unsigned int)) % sizeof(unsigned int), (idx << incr));
    hsh[i / sizeof(unsigned int)] = hsh[i / sizeof(unsigned int)] | (idxs[i] << incr);
    // printf("Updated to %X\n", hsh[i / sizeof(unsigned int)]);
    incr = incr == (CHAR_BIT*(sizeof(unsigned int) - 1)) ? 0 : incr + CHAR_BIT;
  }
  // printf("Final idx: %lu\n", (i / sizeof(unsigned int)) + 1);
  hsh[(i / sizeof(unsigned int)) + 1] = 0x0;
  // printf("%X ", hsh[0]);
  return hsh;
}
