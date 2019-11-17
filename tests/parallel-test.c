#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <hiredis/hiredis.h>

static inline void *initialize_interface() {
  redisContext *ctx = redisConnect("localhost", 8080);
  return (void*)ctx;
}

static inline bool run_store(const char *key, char *value, void *_ctx, void *_table) {
  redisContext *ctx = _ctx;
  redisReply *rep;
  rep = redisCommand(ctx, "set %s %s", key, value);
  return (rep != NULL && strcmp(rep->str, "OK") == 0);
}

static inline char *run_fetch(const char *key, void *_ctx, void *_table) {
  redisContext *ctx = _ctx;
  redisReply *rep;
  rep = redisCommand(ctx, "get %s", key);
  return rep == NULL ? NULL : rep->str;
}

static inline void *initialize_data() {
  return NULL;
}

#include "parallel-test-template.c"
