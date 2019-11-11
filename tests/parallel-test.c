#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <hiredis/hiredis.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <getopt.h>
#include <sys/sysinfo.h>
#include "../lib/random_string.h"

struct item {
  char *key;
  char *value;
  bool set;
  bool set_complete;
  bool get;
  int ctr;
};

struct record {
  bool set;
  bool get;
  int retry_count;
  bool fail;
  struct item *item;
};

struct tdata {
  volatile bool *start;
  volatile bool *thread_ready;
  int qlen;
  int *rec_ctr;
  struct record *records;
  struct item **items;
};

volatile int good_set = 0;
volatile int good_get = 0;
volatile int bad_set = 0;
volatile int bad_get = 0;

void *tfunc(void *_shared) {
  struct tdata *shared = _shared;
  int qlen = shared->qlen;
  struct item **queue = shared->items;
  bool tmp1;
  bool tmp2;
  redisReply *rep;
  int rec_ctr;
  int retry_count;
  redisContext *ctx = redisConnect("localhost", 8080);
  *(shared->thread_ready) = true;
  while (!__atomic_load_n(shared->start, __ATOMIC_SEQ_CST)) {}
  for (int i = 0; i < qlen; i++) {
    tmp1 = false;
    retry_count = 0;
    tmp2 = false;
    if (__atomic_compare_exchange_n(&(queue[i]->set), &tmp1, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      // Set was false (is now true)
      rep = redisCommand(ctx, "set %s %s", queue[i]->key, queue[i]->value);
      rec_ctr = __atomic_fetch_add(shared->rec_ctr, 1, __ATOMIC_SEQ_CST);
      shared->records[rec_ctr].set = true;
      shared->records[rec_ctr].item = queue[i];
      __atomic_store_n(&(queue[i]->set_complete), true, __ATOMIC_SEQ_CST);
      if (rep == NULL || strcmp(rep->str, "OK") != 0) {
        __atomic_add_fetch(&bad_set, 1, __ATOMIC_SEQ_CST);
      } else {
        __atomic_add_fetch(&good_set, 1, __ATOMIC_SEQ_CST);
      }
      freeReplyObject(rep);
    } else if (__atomic_compare_exchange_n(&(queue[i]->get), &tmp2, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      // Get was false (is now true)
      while (__atomic_load_n(&(queue[i]->set_complete), __ATOMIC_SEQ_CST) == false) {sched_yield();}
      rep = redisCommand(ctx, "get %s", queue[i]->key);
      rec_ctr = __atomic_fetch_add(shared->rec_ctr, 1, __ATOMIC_SEQ_CST);
      shared->records[rec_ctr].get = true;
      shared->records[rec_ctr].item = queue[i];
      if (rep == NULL) {
        printf("AAAAA NULL REP\n");
        __atomic_add_fetch(&bad_get, 1, __ATOMIC_SEQ_CST);
      } else if (strcmp(rep->str, queue[i]->value) != 0) {
        while (strcmp(rep->str, queue[i]->value) != 0 && retry_count < 500) {
          freeReplyObject(rep);
          sched_yield();
          rep = redisCommand(ctx, "get %s", queue[i]->key);
          retry_count++;
        }
        shared->records[rec_ctr].retry_count = retry_count;
        shared->records[rec_ctr].fail = strcmp(rep->str, queue[i]->value) != 0;
        printf("Retry: (%d %s) %s\n", retry_count, shared->records[rec_ctr].fail ? "fail" : "success", rep->str);
        if (!shared->records[rec_ctr].fail) {
          __atomic_add_fetch(&good_get, 1, __ATOMIC_SEQ_CST);
        } else {
          __atomic_add_fetch(&bad_get, 1, __ATOMIC_SEQ_CST);
        }
      } else {
        __atomic_add_fetch(&good_get, 1, __ATOMIC_SEQ_CST);
      }
      freeReplyObject(rep);
    } else {
      printf("wrong\n");
    }
  }
  fflush(stdout);
  redisFree(ctx);
  return NULL;
}

int main(int argc, char *argv[]) {
  int item_count = 100000;
  int thread_count = get_nprocs();
  bool runchecks = false;

  const struct option prog_opts[] = {
    {"skipcheck", optional_argument, NULL, 'r'},
    {"threads", optional_argument, NULL, 't'},
    {"items", optional_argument, NULL, 'c'}
  };
  int c;
  while (1) {
    c = getopt_long(argc, argv, "xtc", prog_opts, NULL);
    if (c == -1) { break; }
    switch (c) {
      case 'r':
        runchecks = true;
        break;
      case 't':
        if (optarg != NULL) {
          thread_count = strtol(optarg, NULL, 10);
        } else {
          printf("-t requires an argument\n");
        }
        break;
      case 'c':
        if (optarg != NULL) {
          item_count = strtol(optarg, NULL, 10);
        } else {
          printf("-c requires an argument\n");
        }
        break;
    }
  }

  printf("Item Count: %d\nThread Count: %d\nRunning Checks? %s\n", item_count, thread_count, runchecks ? "true" : "false");

  int items_per_queue = (item_count / thread_count) * 2;

  int *rec_ctr = malloc(sizeof(int));
  *rec_ctr = 0;
  struct record *records = malloc(sizeof(struct record) * (item_count * 2));
  for (int i = 0; i < item_count; i++) {
    records[i].set = false;
    records[i].get = false;
    records[i].retry_count = 0;
    records[i].fail = false;
    records[i].item = NULL;
  }

  struct item *items = malloc(sizeof(struct item)*item_count);
  struct item *item;
  for (int i = 0; i < item_count; i++) {
    items[i].key = random_string('k');
    items[i].value = random_string('v');
    items[i].set = false;
    items[i].set_complete = false;
    items[i].get = false;
    items[i].ctr = 0;
  }
  struct item **queues[thread_count];
  for (int i = 0; i < thread_count; i++) {
    queues[i] = malloc(sizeof(struct item*) * (items_per_queue + 2));
  }
  int count = 0;
  int idx;
  int qnum = 0;
  int qidx[thread_count];
  for (int i = 0; i < thread_count; i++) {
    qidx[i] = 0;
  }
  while (count < item_count*2) {
    idx = rand() % item_count;
    if (items[idx].ctr < 2) {
      qnum = (qnum + 1) % thread_count;
      queues[qnum][qidx[qnum]] = &items[idx];
      // memcpy(&(queues[qnum][qidx[qnum]]), &(items[idx]), sizeof(struct item));
      qidx[qnum]++;
      items[idx].ctr++;
      count++;
    }
  }
  struct tdata *shared;
  pthread_t thread_ids[thread_count];
  bool *thread_ready = malloc(sizeof(bool)*thread_count);
  bool *start = malloc(sizeof(bool));
  *start = false;
  for (int i = 0; i < thread_count; i++) {
    shared = malloc(sizeof(struct tdata));
    shared->start = start;
    shared->qlen = qidx[i];
    shared->items = queues[i];
    shared->thread_ready = thread_ready + i;
    shared->records = records;
    shared->rec_ctr = rec_ctr;
    pthread_create(&(thread_ids[i]), NULL, tfunc, shared);
  }
  bool all;
  while (true) {
    all = true;
    for (int i = 0; i < thread_count; i++) {
      all = all || thread_ready[i];
    }
    if (all)
      break;
  }
  struct timespec t1, t2;
  clock_gettime(CLOCK_REALTIME, &t1);
  __atomic_store_n(start, true, __ATOMIC_SEQ_CST);
  for (int i = 0; i < thread_count; i++) {
    pthread_join(thread_ids[i], NULL);
  }
  clock_gettime(CLOCK_REALTIME, &t2);
  printf("Run time: %.4fs\n", (t2.tv_sec - t1.tv_sec) + ((t2.tv_nsec - t1.tv_nsec) / 1000000000.0));
  if (runchecks) {
    printf("Checking records...\n");
    int set_ary_ctr = 0;
    bool any;
    int err = 0;
    int fail_ctr = 0;
    float avj_retries = 0;
    char **set_ary = malloc(sizeof(char*)*item_count);
    for (int i = 0; i < item_count * 2; i++) {
      if (records[i].fail) {
        fail_ctr++;
      } else if (records[i].retry_count > 0) {
        avj_retries = ((avj_retries * (i - fail_ctr)) + records[i].retry_count) / ((i - fail_ctr) + 1);
      }
      if (records[i].set) {
        set_ary[set_ary_ctr] = records[i].item->key;
        set_ary_ctr++;
      } else if (records[i].get) {
        any = false;
        for (int j = 0; j < set_ary_ctr; j++) {
          if (strcmp(set_ary[j], records[i].item->key) == 0) {
            any = true;
            break;
          }
        }
        if (!any) {
          err++;
        }
      }
    }
    printf("Record check complete (%d errors)\n", err);
    printf("%d total failures\n%f retries before sucess\n", fail_ctr, avj_retries);
  }
  printf("Set:\n  Good:  %d\n  Bad:   %d\nGet:\n  Good:  %d\n  Bad:   %d\n\n  Total: %d\n", good_set, bad_set, good_get, bad_get, item_count);
  return 0;
}
