#include <stdlib.h>
#include <stdio.h>
#include "template.h"
#include "int.h"

static int update(void**, char*);
const static struct updater overwriter = {
  .name = "overwrite",
  .func = &update,
  .safe = false
};

const struct data_type data_type_int = {
  .name = "int",
  .getter = &get,
  .free_ele = &free_ele,
  .setter = &set,
  .updater_length = 1,
  .updaters = &overwriter,
  .clone = &clone
};

static int free_ele(void *val) {
  free(val);
  return 0;
}

static void *clone(void *oldval) {
  int *newval = malloc(sizeof(int));
  __atomic_store(newval, (int*)oldval, __ATOMIC_SEQ_CST);
  free(oldval);
  return newval;
}

// Return value: 2nd argument (rval)
// Errors:
// 1: rval->val need to be freed
static int get(void* val, struct return_val *rval) {
  rval->value = malloc(sizeof(char)*20);
  int ival = __atomic_load_n((int*)val, __ATOMIC_SEQ_CST);
  snprintf(rval->value, 20, "%d", ival);
  return 1;
}

// Errors: See standard error codes in type.h
// Notes:
//   Setters are responsible for taking a string (command), parsing
// it and then settings it. There is no seperate parser stage.
static int set(void** val, char *raw_set_val) {
  int set_val = strtol(raw_set_val, NULL, 10);
  (*val) = malloc(sizeof(int));
  *((int*)*val) = set_val;
  return 0;
}

static int update(void** val, char *raw_update_val) {
  int update_val = strtol(raw_update_val, NULL, 10);
  (*val) = malloc(sizeof(int));
  *((int*)*val) = update_val;
  return 0;
}
