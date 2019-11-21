#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "template.h"
#include "int.h"

static int update(void**, char**);
const static struct updater overwriter = {
  .name = "overwrite",
  .func = &update,
  .safe = false
};

static void *set(int*, char**);
const static struct setter store = {
  .name = "store",
  .func = &set,
  .safe = false
};

static int get(struct return_val*, void*, char**);
const static struct getter fetch = {
  .name = "fetch",
  .func = &get,
  .safe = false
};

const struct data_type data_type_string = {
  .name = "string",
  .getter_length = 1,
  .setter_length = 1,
  .getters = &fetch,
  .setters = &store,
  .free_ele = &free_ele,
  .updater_length = 1,
  .updaters = &overwriter,
  .clone = &clone
};

static int free_ele(void *val) {
  free(val);
  return 0;
}

static int get(struct return_val *rval, void *val, char **args) {
  rval->value = (char*)val;
  return 0;
}

static void *set(int *errors, char **args) {
  return strdup(args[0]);
}

static void *clone(void *oldval) {
  return strdup((char*)oldval);
}

static int update(void **oldval, char **args) {
  char *tmp = *oldval;
  *oldval = strdup(args[0]);
  free(tmp);
  return 0;
}
