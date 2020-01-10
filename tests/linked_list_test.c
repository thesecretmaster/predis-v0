#include "../lib/linked_list.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/sysinfo.h>

struct tinfo {
  int tnum;
  unsigned int put_count;
  unsigned int max_offset;
  struct linked_list *ll;
  volatile int *last_inserted;
};

struct cinfo {
  int tcnt;
  unsigned int max_offset;
  struct linked_list *ll;
  volatile int *last_inserted_arr;
  volatile int *done;
};

void consistancy_check_forwards(struct linked_list *ll, int tcnt, int *inconsistant_errors, int *consistancy_errors, unsigned int max_offset) {
  struct linked_list_elem *e = get_head(ll);
  bool early_end = false;
  int tctrs[tcnt];
  int ctr = 0;
  for (int i = 0; i < tcnt; i++) {
    tctrs[i] = -1;
  }
  struct linked_list_elem *next;
  while ((next = get_next(ll, e, true)) != get_tail(ll) && next != NULL) {
    if (next->value.deleteme) {
      e = next;
      continue;
    }
    // if (get_prev(ll, get_next(ll, e)) != e) {
    //   inconsistant_ctr++;
    //   continue;
    // } else if (inconsistant_ctr != 0) {
    //   // printf("Inconsistant (%d)\n", inconsistant_ctr);
    //   *inconsistant_errors += inconsistant_ctr;
    // }
    if (tctrs[next->value.thread_num] != next->value.ctr && tctrs[next->value.thread_num] != -1) {
      printf("%s Consistancy error (thread %d, expected %d, got %d (deleted: %s))\n", next->value.ctr > tctrs[next->value.thread_num] ? "GT" : "LT", next->value.thread_num, tctrs[next->value.thread_num], next->value.ctr, next->value.deleteme ? "true" : "false");
      *consistancy_errors = *consistancy_errors + 1;
    }
    tctrs[next->value.thread_num] = next->value.ctr - 1;
    // inconsistant_ctr = 0;
    e = next;
    ctr++;
    if (ctr > max_offset * 2) {
      early_end = true;
      // printf("BREEEEAK\n");
      break;
    }
  }
  release(ll, next);
  if (!early_end) {
    for (int i = 0; i < tcnt; i++) {
      if (tctrs[i] != -1) {
        printf("Thread %d wrong base (%d)\n", i, tctrs[i]);
      }
    }
  }
}

void consistancy_check_reverse(struct linked_list *ll, int tcnt, int *inconsistant_errors, int *consistancy_errors, volatile int *last_inserted_arr, unsigned int max_offset) {
  struct linked_list_elem *head = get_head(ll);
  struct linked_list_elem *base = head;
  struct linked_list_elem *tbase;
  for (int i = 0; i < max_offset * 2; i++) {
    if ((tbase = get_next(ll, base, true)) == get_tail(ll)) {
      break;
    }
    base = tbase;
  }
  struct linked_list_elem *prev;
  int counters[tcnt];
  for (int i = 0; i < tcnt; i++) {
    counters[i] = -9;
  }
  int retries = 0;
  bool autorel = true;
  while ((prev = get_prev(ll, base, autorel)) != head) {
    if (prev == NULL) {
      autorel = false;
      continue;
    } else {
      autorel = true;
    }
    if (prev->value.deleteme) {
      base = prev;
      continue;
    }
    if (counters[prev->value.thread_num] == -9) {
      counters[prev->value.thread_num] = prev->value.ctr;
    }
    if (prev->value.ctr <= last_inserted_arr[prev->value.thread_num]) {
      counters[prev->value.thread_num] = -10;
    }
    if (counters[prev->value.thread_num] != prev->value.ctr && counters[prev->value.thread_num] != -10) {
      retries++;
      if (retries < 100000) {
        release(ll, prev);
        // printf("Retrying (%d, %d)\n", counters[prev->value.thread_num], prev->value.ctr);
        autorel = false;
        sched_yield();
        continue;
      } else {
        printf("Too many %d %d\n", counters[prev->value.thread_num], prev->value.ctr);
        counters[prev->value.thread_num] = prev->value.ctr;
      }
    }
    autorel = true;
    if (retries > 0) {
      printf("Retries: %d\n", retries);
      *consistancy_errors = *consistancy_errors + 1;
      retries = 0;
    }
    if (counters[prev->value.thread_num] != -10) {
      counters[prev->value.thread_num] = counters[prev->value.thread_num] + 1;
    }
    base = prev;
  }
  // struct linked_list_elem *e = ll->tail;
  // e->value.ctr = -1;
  // struct linked_list_elem *prevs[tcnt];
  // // int tctrs[tcnt];
  // // int retries;
  // for (int i = 0; i < tcnt; i++) {
  //   prevs[i] = e;
  // }
  // // retries = 0;
  // struct linked_list_elem *ne;
  // struct linked_list_elem *pne = NULL;
  // while ((ne = get_prev(ll, e)) != NULL) {
  //   __atomic_thread_fence(__ATOMIC_SEQ_CST);
  //   if (prevs[ne->value.thread_num]->value.ctr + 1 != ne->value.ctr) {
  //     if (pne != ne) {
  //       pne = ne;
  //       continue;
  //     } else {
  //       printf("Too many retries\n");
  //     }
  //   }
  //   if (retries > 0) {
  //     *consistancy_errors = *consistancy_errors + 1;
  //     printf("Consistancy error (expected %d, got %d) (%d retries)\n", prevs[ne->value.thread_num]->value.ctr, ne->value.ctr, retries);
  //     retries = 0;
  //   }
  //   __atomic_thread_fence(__ATOMIC_SEQ_CST);
  //   prevs[ne->value.thread_num] = ne;
  //   e = ne;
  // }
}

void *consistancy_check_forewards_thread(void *_info) {
  struct cinfo *info = _info;
  int tcnt = info->tcnt;
  unsigned int max_offset = info->max_offset;
  int consistancy_errors = 0;
  int inconsistant_errors = 0;
  struct linked_list *ll = info->ll;
  int check_ctr = 0;
  while (*info->done < 0) {
    check_ctr++;
    consistancy_check_forwards(ll, tcnt, &inconsistant_errors, &consistancy_errors, max_offset);
    sched_yield();
  }
  __atomic_add_fetch(info->done, 1, __ATOMIC_SEQ_CST);
  printf("Ran %d forewards consistancy checks (%d inconst errors, %d const errors)\n", check_ctr, inconsistant_errors, consistancy_errors);
  return NULL;
}

void *consistancy_check_reverse_thread(void *_info) {
  struct cinfo *info = _info;
  int tcnt = info->tcnt;
  unsigned int max_offset = info->max_offset;
  volatile int *last_inserted_arr = info->last_inserted_arr;
  int consistancy_errors = 0;
  int inconsistant_errors = 0;
  struct linked_list *ll = info->ll;
  int check_ctr = 0;
  while (*info->done < 0) {
    check_ctr++;
    consistancy_check_reverse(ll, tcnt, &inconsistant_errors, &consistancy_errors, last_inserted_arr, max_offset);
  }
  __atomic_add_fetch(info->done, 1, __ATOMIC_SEQ_CST);
  printf("Ran %d reverse consistancy checks (%d inconst errors, %d const errors)\n", check_ctr, inconsistant_errors, consistancy_errors);
  return NULL;
}

void *deleter_thread(void *_info) {
  struct cinfo *info = _info;
  struct linked_list *ll = info->ll;
  struct linked_list_elem *after_node;
  struct linked_list_elem *before_node;
  struct llval value;
  int delctr = 0;
  value.deleteme = true;
  value.ctr = -5;
  value.thread_num = -1;
  while (*info->done < 0) {
    after_node = get_next(ll, get_head(ll), true);
    before_node = get_prev(ll, clone_ref(ll, after_node), true);
    if (before_node == NULL) {
      continue;
    }
    delctr++;
    insert_after(ll, before_node, value);
    release(ll, before_node);
    sched_yield();
    while (after_node != get_head(ll) && !after_node->value.deleteme) {
      after_node = get_prev(ll, after_node, true);
    }
    delete_elem(ll, after_node);
    release(ll, after_node);
  }
  printf("%d delete tests\n", delctr);
  __atomic_add_fetch(info->done, 1, __ATOMIC_SEQ_CST);
  return NULL;
}

void *appender_thread(void *_info) {
  struct tinfo *info = _info;
  struct linked_list_elem *start;
  unsigned int max_offset = info->max_offset;
  int offset;
  int failed_inserts = 0;
  int successful_inserts = 0;
  // int check_offset = ((CHECK_FREQ / 10) / 2) - (rand() % (CHECK_FREQ / 10));
  struct linked_list *ll = info->ll;
  struct llval values;
  struct linked_list_elem *tmpstart;
  bool finish_early;
  for (int i = 0; i < info->put_count; i++) {
    values.ctr = i;
    values.thread_num = info->tnum;
    values.deleteme = false;
    // sched_yield();
    // if (i % (CHECK_FREQ + check_offset) == 0) {
    //   consistancy_check(ll, info->tnum, i - 1);
    // }
    start = get_head(ll);
    offset = (get_length(ll) > max_offset ? max_offset : get_length(ll));
    offset = offset == 0 ? 0 : rand() % offset;
    finish_early = false;
    for (int j = 0; j < offset; j++) {
      tmpstart = get_next(ll, start, true);
      if (tmpstart == NULL || tmpstart == get_tail(ll) || tmpstart->value.thread_num == info->tnum) {
        release(ll, tmpstart);
        finish_early = true;
        break;
      }
      start = tmpstart;
    }
    if (start == NULL) {
      continue;
    }
    if (insert_after(ll, start, values) != 0) {
      i -= 1;
      failed_inserts++;
      continue;
    } else {
      successful_inserts++;
    }
    if (!finish_early) {
      release(ll, start);
    }
    *info->last_inserted = i;
  }
  printf("%d failed / %d sucessful inserts\n", failed_inserts, successful_inserts);
  return NULL;
}

int main(int argc, char *argv[]) {
  unsigned int thread_put_count = 1000000;
  unsigned int thread_cnt = get_nprocs();
  unsigned int max_offset = 10;

  const struct option prog_opts[] = {
    {"threads", optional_argument, NULL, 't'},
    {"count", optional_argument, NULL, 'c'},
    {"offset", optional_argument, NULL, 'o'}
  };
  int c;
  while (1) {
    c = getopt_long(argc, argv, "t:c:o:", prog_opts, NULL);
    if (c == -1) { break; }
    switch (c) {
      case 't':
        thread_cnt = strtol(optarg, NULL, 10);
        break;
      case 'c':
        thread_put_count = strtol(optarg, NULL, 10);
        break;
      case 'o':
        max_offset = strtol(optarg, NULL, 10);
        break;
    }
  }

  thread_cnt = thread_cnt > 4 ? thread_cnt - 3 : thread_cnt;

  printf("%d threads...\n", thread_cnt);

  struct linked_list *ll = initialize_ll();
  struct cinfo cinf;
  volatile int last_inserted_arr[thread_cnt];
  for (int i = 0; i < thread_cnt; i++) {
    last_inserted_arr[i] = 0;
  }
  cinf.tcnt = thread_cnt;
  cinf.ll = ll;
  cinf.max_offset = max_offset;
  cinf.last_inserted_arr = last_inserted_arr;
  cinf.done = malloc(sizeof(int));
  *cinf.done = -1;
  struct tinfo *info;
  pthread_t cinf_fore_tid;
  pthread_t cinf_rev_tid;
  pthread_t cinf_del_tid;
  pthread_t tids[thread_cnt];
  for (int i = 0; i < thread_cnt; i++) {
    info = malloc(sizeof(struct tinfo));
    info->tnum = i;
    info->max_offset = max_offset;
    info->ll = ll;
    info->put_count = thread_put_count;
    info->last_inserted = &last_inserted_arr[i];
    pthread_create(&tids[i], NULL, appender_thread, info);
  }
  pthread_create(&cinf_fore_tid, NULL, consistancy_check_forewards_thread, &cinf);
  pthread_create(&cinf_rev_tid, NULL, consistancy_check_reverse_thread, &cinf);
  pthread_create(&cinf_del_tid, NULL, deleter_thread, &cinf);
  for (int i = 0; i < thread_cnt; i++) {
    pthread_join(tids[i], NULL);
  }
  *cinf.done = 0;
  pthread_join(cinf_del_tid, NULL);
  printf("FORE AND REV\n");
  pthread_join(cinf_fore_tid, NULL);
  pthread_join(cinf_rev_tid, NULL);
  printf("ALL JOINED\n");
  while (*cinf.done < 3) {}
  struct linked_list_elem *ptr = get_next(ll, get_head(ll), false);
  int *foreward_order = malloc(sizeof(int) * (thread_put_count * thread_cnt * 2 + 10 /* delete thread padding */));
  int i = 0;
  struct linked_list_elem *next;
  while ((next = get_next(ll, ptr, true)) != get_tail(ll)) {
    if (next->value.deleteme == true) {
      ptr = next;
      continue;
    }
    foreward_order[i*2 + 0] = next->value.thread_num;
    foreward_order[i*2 + 1] = next->value.ctr;
    i++;
    // printf("thread %d, num %d\n", ptr->value[1], ptr->value[0]);
    ptr = next;
  }
  // release(ll, ptr);
  printf("Running integrity test...\n");
  ptr = get_tail(ll);
  i--;
  int sadcount = 0;
  struct linked_list_elem *prev;
  int newpcount = 0;
  while ((prev = get_prev(ll, ptr, true)) != get_head(ll)) {
    if (prev->value.deleteme == true) {
      // raise(SIGABRT);
      ptr = prev;
      printf("This shouldn't hpappen\n");
      continue;
    }
    if (prev->refcount != 1) {
      sadcount++;
      printf("Ewwwww %d\n", prev->refcount);
    }
    if (prev->value.thread_num != foreward_order[i*2 + 0] || prev->value.ctr != foreward_order[i*2 + 1]) {
      newpcount++;
    }
    i--;
    ptr = prev;
  }
  printf("Newp %d\n", newpcount);
  printf("Sadcount: %d\n", sadcount);
  // release(ll, ptr);
  printf("Final consitancy checks...\n");
  int consistancy_errors;
  int inconsistant_errors;
  consistancy_errors = 0;
  inconsistant_errors = 0;
  consistancy_check_forwards(ll, thread_cnt, &inconsistant_errors, &consistancy_errors, max_offset);
  printf("Forewards: %d inconst, %d const\n", inconsistant_errors, consistancy_errors);
  consistancy_errors = 0;
  inconsistant_errors = 0;
  consistancy_check_reverse(ll, thread_cnt, &inconsistant_errors, &consistancy_errors, last_inserted_arr, max_offset);
  printf("Reverse: %d inconst, %d const\n", inconsistant_errors, consistancy_errors);
  return 0;
}
