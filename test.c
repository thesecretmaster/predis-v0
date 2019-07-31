#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/sysinfo.h>
#include "predis.h"

struct sharedstruct {
  struct thread_info_list *tilist;
  struct thread_info_list *ti_ele;
  int test_count;
  int test_rn_count;
  int thread_count;
  struct main_struct *ms;
  int tid;
  volatile short int weird_lock;
};

static volatile bool start_threads = false;

void* dostuff(void*);

int main(int argc, char* argv[]) {
  int thread_count = get_nprocs();
  int test_count = 100;
  int test_rn_count = 2000;

  const struct option prog_opts[] = {
    {"threads", optional_argument, NULL, 'a'},
    {"tests", optional_argument, NULL, 'b'},
    {"numbers", optional_argument, NULL, 'c'}
  };
  int c;
  while (1) {
    c = getopt_long(argc, argv, "", prog_opts, NULL);
    if (c == -1) { break; }
    switch (c) {
      case 'a':
        thread_count = strtol(optarg, NULL, 10);
        break;
      case 'b':
        test_count = strtol(optarg, NULL, 10);
        break;
      case 'c':
        test_rn_count = strtol(optarg, NULL, 10);
        break;
    }
  }

  // So... 2 appears to be the magic number at which we're unlikly
  // to hit a deadlock condition. If we set the size to be too low
  // then the main_struct will fill up before any of the threads
  // exit the first `set` phase, and they'll all be stuck trying
  // to append to an already full container.
  // You might do alright with 3 iff THREAD_COUNT <= 6
  int main_len = (test_rn_count-(pow(thread_count, 2)-1))*thread_count;

  struct main_struct *ms = init(main_len);

  struct sharedstruct sharedstruct = (struct sharedstruct){
    .ms = ms,
    .thread_count = thread_count,
    .test_count = test_count,
    .test_rn_count = test_rn_count
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

  printf("Getting this party started\n");
  start_threads = true;

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

int **genRnSets(int set_count, int set_length) {
  int **sets = malloc(sizeof(int*)*set_count);
  for (int i = 0; i < set_count; i++) {
    sets[i] = malloc(sizeof(int)*set_length);
    for (int j = 0; j < set_length; j++) {
      sets[i][j] = rand();
    }
  }
  return sets;
}

void *dostuff(void *ptr) {
  struct sharedstruct *sharedstruct = ptr;
  struct main_struct *ms = sharedstruct->ms;
  struct thread_info_list *tilist = sharedstruct->tilist;
  struct thread_info_list *ti = sharedstruct->ti_ele;
  int tid = sharedstruct->tid;
  safe = &(ti->safe);
  sharedstruct->weird_lock = 0;

  int *idx = malloc(sizeof(int)*sharedstruct->test_rn_count);
  char str[20];
  int wrongcount = 0;
  int seterrcount = 0;
  int geterrcount = 0;
  int goodcount = 0;
  int geterrors;
  int** rnsets = genRnSets(sharedstruct->test_count, sharedstruct->test_rn_count);

  while (start_threads == false) {}
  // int ti_ctr;
  // struct thread_info_list *ti_ptr = NULL;
  struct return_val* rval = malloc(sizeof(struct return_val));
  for (int i = 0; i < sharedstruct->test_count; i++) {
    for (int j = 0; j < sharedstruct->test_rn_count; j++) {
      snprintf(str, 20, "%d", rnsets[i][j]);
      before_set:
      idx[j] = set("int", ms, str);
      if (idx[j] < 0) {
        seterrcount++;
        // printf("%d: Errors: %d\n", tid, seterrcount);
        sched_yield();
        clean_queue(ms);
        goto before_set;
        // printf("Set: %d\n", idx[j]);
      }
    }
    for (int j = 0; j < sharedstruct->test_rn_count; j++) {
      if (idx[j] < 0) { continue; }
      snprintf(str, 20, "%d", rnsets[i][j]);
      geterrors = get("int", ms, rval, idx[j]);
      del(ms, idx[j]);
      if (geterrors < 0) {
        geterrcount++;
        // printf("Get: %d\n", geterrors);
      }
      if (strcmp(rval->value, str) != 0) {
        printf("%s != %s\n", rval->value, str);
        wrongcount++;
      } else {
        goodcount++;
      }
      if (geterrors == 1) {
        free(rval->value);
      }
    }
    clean_queue(ms);
    // ti_ctr = 0;
    // ti_ptr = tilist;
    // while (ti_ptr != NULL) {
    //   printf("Thread %d: %lld\n", ti_ctr, ti_ptr->op_counter);
    //   ti_ctr++;
    //   ti_ptr = ti_ptr->next;
    // }
  }

  for (int i = 0; i < sharedstruct->test_count; i++) {
    free(rnsets[i]);
  }
  free(rnsets);
  free(idx);
  free(rval);

  printf("%d: %d wrong\t%d good\t%d errors in set\t%d errors in get\n", tid, wrongcount, goodcount, seterrcount, geterrcount);

  // int a = set("int", ms, "123");
  // int b = set("int", ms, "543");
  // int c = set("int", ms, "13333");
  // struct return_val* rval = malloc(sizeof(struct return_val));
  // rval->value = NULL;
  // get("int", ms, rval, a);
  // int ra = strtol(rval->value, NULL, 10);
  // get("int", ms, rval, b);
  // int rb = strtol(rval->value, NULL, 10);
  // get("int", ms, rval, c);
  // int rc = strtol(rval->value, NULL, 10);
  // printf("a: %d\nb: %d\nc: %d\n", ra, rb, rc);
  return NULL;
}
