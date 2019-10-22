#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/sysinfo.h>
#include "../predis.h"

struct sharedstruct {
  struct thread_info_list *tilist;
  struct thread_info_list *ti_ele;
  int test_count;
  int thread_count;
  int idx;
  struct main_struct *ms;
  int tid;
  volatile short int weird_lock;
};

static volatile bool start_threads = false;
static volatile int complete_threads = 0;
#define MAX_STR_LENGTH 30
#define MIN_STR_LENGTH 5

void* dostuff(void*);

char *genString() {
  int length = (rand() % (MAX_STR_LENGTH-MIN_STR_LENGTH)) + MIN_STR_LENGTH;
  char chr = (rand() % (126-32)) + 32;
  char *str = malloc(sizeof(char)*length);
  for (int i = 0; i < length-1; i++) {
    str[i] = chr;
  }
  str[length-1] = '\0';
  return str;
}

int main(int argc, char* argv[]) {
  int thread_count = get_nprocs();
  int test_count = 100;

  const struct option prog_opts[] = {
    {"threads", optional_argument, NULL, 'a'},
    {"count", optional_argument, NULL, 'c'}
  };
  int c;
  while (1) {
    c = getopt_long(argc, argv, "", prog_opts, NULL);
    if (c == -1) { break; }
    switch (c) {
      case 'a':
        thread_count = strtol(optarg, NULL, 10);
        break;
      case 'c':
        test_count = strtol(optarg, NULL, 10);
        break;
    }
  }

  struct main_struct *ms = init(10);

  int idx = set("string", ms, genString());

  struct sharedstruct sharedstruct = (struct sharedstruct){
    .ms = ms,
    .thread_count = thread_count,
    .test_count = test_count,
    .idx = idx
  };
  pthread_t tid[thread_count];
  struct thread_info_list *ti_ele;
  for (int i = 0; i < thread_count; i++) {
    ti_ele = register_thread(ms);
    sharedstruct.tilist = ms->thread_list;
    sharedstruct.ti_ele = ti_ele;
    sharedstruct.tid = i;
    sharedstruct.weird_lock = 1;
    pthread_create(&tid[i], NULL, &dostuff, &sharedstruct);
    while (sharedstruct.weird_lock != 0) {}
  }

  struct return_val *rval = malloc(sizeof(struct return_val));

  printf("Getting this party started\n");
  start_threads = true;

  int errors = 0;
  int wrong = 0;
  int i;
  char chr;
  while (complete_threads < thread_count) {
    errors = get("string", ms, rval, idx);
    if (errors < 0) { errors++; }
    i = 1;
    chr = rval->value[0];
    while (rval->value[i] != '\0') {
      if (rval->value[i] != chr) {
        printf("WRONG: expected %c but got %c. Full: %s\n", chr, rval->value[i], rval->value);
        wrong++;
        break;
      }
      i++;
    }
  }

  printf("FINAL: wrong: %d, errors %d\n", wrong, errors);
  for (int i = 0; i < thread_count; i++) {
    pthread_join(tid[i], NULL);
  }

  printf("Cleanup!\n");
  int tcount = 0;
  struct thread_info_list *tiptr = ms->thread_list;
  while (1) {
    if (tiptr == NULL) {break;}
    // printf("Thread %d: %s\n", tcount, tiptr->safe ? "true" : "false");
    tcount++;
    tiptr = tiptr->next;
  }
  printf("Used %d threads\n", tcount);
  free_predis(ms);
  return 0;
}

void *dostuff(void *ptr) {
  struct sharedstruct *sharedstruct = ptr;
  struct main_struct *ms = sharedstruct->ms;
  struct thread_info_list *tilist = sharedstruct->tilist;
  struct thread_info_list *ti = sharedstruct->ti_ele;
  int tid = sharedstruct->tid;
  int test_count = sharedstruct->test_count;
  int idx = sharedstruct->idx;
  safe = &(ti->safe);
  sharedstruct->weird_lock = 0;

  while (start_threads == false) {}

  int errors = 0;
  char *str;
  for (int i = 0; i < test_count; i++) {
    str = genString();
    if (update("string", "overwrite", ms, str, idx) < 0) {
      errors++;
    }
    free(str);
  }

  __atomic_add_fetch(&complete_threads, 1, __ATOMIC_SEQ_CST);

  printf("%d: %d errors\n", tid, errors);

  return NULL;
}
