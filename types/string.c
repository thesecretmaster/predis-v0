#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "template.h"
#include "int.h"

static int update(void**, char*);

const static struct updater overwriter = {
  .name = "overwrite",
  .func = &update,
  .safe = false
};

const struct data_type data_type_string = {
  .name = "string",
  .getter = &get,
  .setter = &set,
  .updater_length = 1,
  .updaters = &overwriter,
  .clone = &clone
};

static int get(void *val, struct return_val *rval) {
  rval->value = (char*)val;
  return 0;
}

static int set(void **valptr, char *newval) {
  *valptr = strdup(newval);
  return 0;
}

static void *clone(void *oldval) {
  return strdup((char*)oldval);
}

static int update(void **oldval, char *newval) {
  *oldval = newval;
  return 0;
}
