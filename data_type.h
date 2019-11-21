#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>

struct return_val {
  char* value;
};

struct data_type {
  char *name;
  void* (*clone)(void*);
  int (*free_ele)(void*);
  int updater_length;
  int getter_length;
  int setter_length;
  const struct updater *updaters;
  const struct setter *setters;
  const struct getter *getters;
};

struct updater {
  char *name;
  bool safe;
  int (*func)(void **value /* They can change things, but they can't realloc the whole thing */, char **args);
};

struct setter {
  char *name;
  bool safe;
  void* (*func)(int *errors /* default 0 */, char **args); // Returns the allocated element
};

struct getter {
  char *name;
  bool safe;
  int (*func)(struct return_val *rval, void *value, char **args);
};

#endif // DATA_TYPE_H
