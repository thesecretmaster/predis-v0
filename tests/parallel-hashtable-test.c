#define HT_VAL_TYPE char*
#include "../lib/hashtable.h"
#include <stdlib.h>
#include <stdbool.h>
#include "parallel-test-template.c"

static inline void *initialize_interface() {
  return NULL;
}

static inline bool run_store(const char *key, char *value, void *_ctx, void *_table) {
  struct ht_table *table = _table;
  int rval = ht_store(table, key, &value);
  return (rval == 0);
}

static inline char *run_fetch(const char *key, void *_ctx, void *_table) {
  struct ht_table *table = _table;
  return *ht_find(table, key);
}

static int initEle(char **v) {
  *v = NULL;
  return 0;
}

static inline void *initialize_data() {
  return (void*)ht_init(8, &initEle);
}
