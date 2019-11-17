#define HT_VAL_TYPE char
#include "../lib/hashtable.h"
#include <stdlib.h>

static inline void *initialize_interface() {
  // HT:
  return NULL;
  // Redis:
  // redisContext *ctx = redisConnect("localhost", 8080);
  // return (void*)ctx;
}

static inline bool run_store(const char *key, char *value, void *_ctx, void *_table) {
  // HT:
  struct ht_table *table = _table;
  int rval = ht_store(table, key, value);
  return (rval == 0);
  // Redis:
  // redisContext *ctx = _ctx;
  // redisReply *rep;
  // rep = redisCommand(ctx, "set %s %s", queue[i]->key, queue[i]->value);
  // return (rep != NULL && strcmp(rep->str, "OK") == 0);
}

static inline char *run_fetch(const char *key, void *_ctx, void *_table) {
  // HT:
  struct ht_table *table = _table;
  return ht_find(table, key);
  // Redis:
  // redisContext *ctx = _ctx;
  // redisReply *rep;
  // rep = redisCommand(ctx, "get %s", queue[i]->key);
  // return rep == NULL ? NULL : rep->str;
}

static inline void *initialize_data() {
  // HT:
  return (void*)ht_init(8);
  // Redis:
  return NULL;
}

#include "parallel-test-template.c"
