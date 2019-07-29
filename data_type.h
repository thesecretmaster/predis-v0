#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>

struct return_val {
  char* value;
};

struct data_type {
  char *name;
  int (*setter)(void**, char*);
  void* (*clone)(void*);
  int updater_length;
  const struct updater *updaters;
  int (*getter)(void*, struct return_val*);
};

struct updater {
  char *name;
  bool safe;
  int (*func)(void**, char*);
};

#endif // DATA_TYPE_H
