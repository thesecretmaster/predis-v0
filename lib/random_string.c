#include <stdlib.h>

static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

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
