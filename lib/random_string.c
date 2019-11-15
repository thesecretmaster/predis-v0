#include <stdlib.h>
#include <stdio.h>
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
  *mutlen = len_div_4;
  unsigned int *hsh = malloc(sizeof(unsigned int)*(len_div_4));
  // printf("Allocating hash size %d\n", len_div_4 + 1);
  int i;
  int incr = 0;
  for (i = 0; i < length - 1; i++) {
    // printf("Touching idx %d(+%d), byte %d (%X)\n", i / 16, incr, (i / 4) % 4, (idxs[i] << incr));
    hsh[i / 16] = hsh[i / 16] | (idxs[i] << incr);
    incr = incr == 24*5 ? 0 : incr + 8;
  }
  // printf("Final idx: %d\n", (i / 16) + 1);
  // hsh[(i / 16) + 1] = 0x0;
  // printf("Hash: %X\n", hsh[0]);
  return hsh;
}
