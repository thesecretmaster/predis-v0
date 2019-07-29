#include <stdlib.h>
#include <stdio.h>
#include "template.h"
#include "int.h"

const struct data_type data_type_int = {
  .name = "int",
  .getter = &get,
  .setter = &set
};

// Return value: 2nd argument (rval)
// Errors:
static int get(void* val, struct return_val *rval, int idx) {
  rval->value = malloc(sizeof(char)*20);
  snprintf(rval->value, 20, "%d", *((int*)val));
  return 0;
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
