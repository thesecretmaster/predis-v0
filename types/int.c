#include <stdlib.h>
#include <stdio.h>
#include "template.h"
#include "int.h"

static int update(void**, char**);
const static struct updater overwriter = {
  .name = "overwrite",
  .func = &update,
  .safe = false
};

static int get(struct return_val*, void*, char**);
const static struct getter fetch = {
  .name = "fetch",
  .func = &get,
  .safe = false
};

static void *set(int *errors, char **args);

const static struct getter *getters[] = {&fetch};
const static struct updater *updaters[] = {&overwriter};

const struct data_type data_type_int = {
  .name = "int",
  .getter_length = 1,
  .getters = getters,
  .free_ele = &free_ele,
  .initializer_safe = false,
  .initializer = &set,
  .updater_length = 1,
  .updaters = updaters,
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
static int get(struct return_val *rval, void* val, char **args) {
  rval->value = malloc(sizeof(char)*20);
  int ival = __atomic_load_n((int*)val, __ATOMIC_SEQ_CST);
  snprintf(rval->value, 20, "%d", ival);
  return 1;
}

// Errors: See standard error codes in type.h
// Notes:
//   Setters are responsible for taking a string (command), parsing
// it and then settings it. There is no seperate parser stage.
static void *set(int *errors, char **args) {
  char *raw_set_val = args[0];
  int set_val = strtol(raw_set_val, NULL, 10);
  int *val = malloc(sizeof(int));
  *val = set_val;
  return val;
}

// Build a helper which auto returns argument errors
// Also we have no arglength?
static int update(void** _val, char **args) {
  int *val = *_val;
  char *raw_update_val = args[0];
  int update_val = strtol(raw_update_val, NULL, 10);
  __atomic_store_n(val, update_val, __ATOMIC_SEQ_CST);
  return 0;
}
