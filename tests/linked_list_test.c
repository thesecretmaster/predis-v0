#include "../types/linked_list.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <stdio.h>

struct tinfo {
  int tnum;
  struct linked_list *ll;
  volatile int *last_inserted;
};

struct cinfo {
  int tcnt;
  struct linked_list *ll;
  volatile int *last_inserted_arr;
  volatile bool *done;
};

#define THREAD_PUT_CNT 1000000
#define THREAD_CNT 7
// #define CHECK_FREQ 5000

// void consistancy_check(struct linked_list *ll, int tnum, int i) {
//   return;
//   struct linked_list_elem *e = get_next(ll, ll->head);
//   int inconsistant_ctr = 0;
//   while (get_next(ll, e) != NULL) {
//     if (get_prev(ll, get_next(ll, e)) != e) {
//       inconsistant_ctr++;
//       printf("Inconsistant (%d)\n", inconsistant_ctr);
//       continue;
//     }
//     if (e->value[1] == tnum) {
//       if (e->value[0] == i) {
//         i--;
//       } else {
//         printf("Consistancy error!\n");
//       }
//     }
//     inconsistant_ctr = 0;
//     e = get_next(ll, e);
//   }
//   if (i != -1) {
//     printf("MOar error %d\n", i);
//   }
// }

void consistancy_check_forwards(struct linked_list *ll, int tcnt, int *inconsistant_errors, int *consistancy_errors) {
  struct linked_list_elem *e = get_next(ll, get_head(ll));
  // int inconsistant_ctr = 0;
  bool early_end = false;
  int tctrs[tcnt];
  int ctr = 0;
  for (int i = 0; i < tcnt; i++) {
    tctrs[i] = -1;
  }
  while (get_next(ll, e) != NULL) {
    // if (get_prev(ll, get_next(ll, e)) != e) {
    //   inconsistant_ctr++;
    //   continue;
    // } else if (inconsistant_ctr != 0) {
    //   // printf("Inconsistant (%d)\n", inconsistant_ctr);
    //   *inconsistant_errors += inconsistant_ctr;
    // }
    if (tctrs[e->value.thread_num] != e->value.ctr && tctrs[e->value.thread_num] != -1) {
      // printf("Consistancy error\n");
      *consistancy_errors = *consistancy_errors + 1;
    }
    tctrs[e->value.thread_num] = e->value.ctr - 1;
    // inconsistant_ctr = 0;
    e = get_next(ll, e);
    ctr++;
    if (ctr > 100) {
      early_end = true;
      break;
    }
  }
  if (!early_end) {
    for (int i = 0; i < tcnt; i++) {
      if (tctrs[i] != -1) {
        printf("Thread %d wrong base (%d)\n", i, tctrs[i]);
      }
    }
  }
}

void consistancy_check_reverse(struct linked_list *ll, int tcnt, int *inconsistant_errors, int *consistancy_errors, volatile int *last_inserted_arr) {
  struct linked_list_elem *base = get_tail(ll);
  struct linked_list_elem *head = get_head(ll);
  // In theory, we should have `volatile int *last_inserted[tcnt]`
  // for (int i = 0; i < 100; i++) {
  //   head = get_next(ll, ll->head);
  // }
  struct linked_list_elem *prev;
  int counters[tcnt];
  for (int i = 0; i < tcnt; i++) {
    counters[i] = 0;
  }
  int retries = 0;
  while ((prev = get_prev(ll, base)) != head) {
    if (prev->value.ctr <= last_inserted_arr[prev->value.thread_num]) {
      counters[prev->value.thread_num] = -1;
    }
    if (counters[prev->value.thread_num] != prev->value.ctr && counters[prev->value.thread_num] != -1) {
      retries++;
      if (retries < 10000) {
        continue;
      } else {
        printf("Too many %d %d\n", counters[prev->value.thread_num], prev->value.ctr);
        counters[prev->value.thread_num] = prev->value.ctr;
      }
    }
    if (retries > 0) {
      printf("Retries: %d\n", retries);
      *consistancy_errors = *consistancy_errors + 1;
      retries = 0;
    }
    if (counters[prev->value.thread_num] != -1) {
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
  int consistancy_errors = 0;
  int inconsistant_errors = 0;
  struct linked_list *ll = info->ll;
  int check_ctr = 0;
  while (!*info->done) {
    check_ctr++;
    consistancy_check_forwards(ll, tcnt, &inconsistant_errors, &consistancy_errors);
    sched_yield();
  }
  printf("Ran %d forewards consistancy checks (%d inconst errors, %d const errors)\n", check_ctr, inconsistant_errors, consistancy_errors);
  return NULL;
}

void *consistancy_check_reverse_thread(void *_info) {
  struct cinfo *info = _info;
  int tcnt = info->tcnt;
  volatile int *last_inserted_arr = info->last_inserted_arr;
  int consistancy_errors = 0;
  int inconsistant_errors = 0;
  struct linked_list *ll = info->ll;
  int check_ctr = 0;
  while (!*info->done) {
    check_ctr++;
    consistancy_check_reverse(ll, tcnt, &inconsistant_errors, &consistancy_errors, last_inserted_arr);
  }
  printf("Ran %d reverse consistancy checks (%d inconst errors, %d const errors)\n", check_ctr, inconsistant_errors, consistancy_errors);
  return NULL;
}

void *appender_thread(void *_info) {
  struct tinfo *info = _info;
  struct linked_list_elem *start;
  int offset;
  // int check_offset = ((CHECK_FREQ / 10) / 2) - (rand() % (CHECK_FREQ / 10));
  struct linked_list *ll = info->ll;
  struct llval values;
  struct linked_list_elem *tmpstart;
  for (int i = 0; i < THREAD_PUT_CNT; i++) {
    values.ctr = i;
    values.thread_num = info->tnum;
    // sched_yield();
    // if (i % (CHECK_FREQ + check_offset) == 0) {
    //   consistancy_check(ll, info->tnum, i - 1);
    // }
    start = get_head(ll);
    offset = (get_length(ll) > 10 ? 10 : get_length(ll));
    offset = offset == 0 ? 0 : rand() % offset;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    for (int j = 0; j < offset; j++) {
      if ((tmpstart = get_next(ll, start))->value.thread_num == info->tnum) {
        break;
      }
      start = tmpstart;
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    insert_after(ll, start, values);
    *info->last_inserted = i;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
  }
  return NULL;
}

int main() {
  struct linked_list *ll = initialize_ll();
  struct cinfo cinf;
  volatile int last_inserted_arr[THREAD_CNT];
  for (int i = 0; i < THREAD_CNT; i++) {
    last_inserted_arr[i] = 0;
  }
  cinf.tcnt = THREAD_CNT;
  cinf.ll = ll;
  cinf.last_inserted_arr = last_inserted_arr;
  cinf.done = malloc(sizeof(bool));
  *cinf.done = false;
  struct tinfo *info;
  pthread_t cinf_fore_tid;
  pthread_t cinf_rev_tid;
  pthread_t tids[THREAD_CNT];
  for (int i = 0; i < THREAD_CNT; i++) {
    info = malloc(sizeof(struct tinfo));
    info->tnum = i;
    info->ll = ll;
    info->last_inserted = &last_inserted_arr[i];
    pthread_create(&tids[i], NULL, appender_thread, info);
  }
  pthread_create(&cinf_fore_tid, NULL, consistancy_check_forewards_thread, &cinf);
  pthread_create(&cinf_rev_tid, NULL, consistancy_check_reverse_thread, &cinf);
  for (int i = 0; i < THREAD_CNT; i++) {
    pthread_join(tids[i], NULL);
  }
  *cinf.done = true;
  pthread_join(cinf_fore_tid, NULL);
  pthread_join(cinf_rev_tid, NULL);
  struct linked_list_elem *ptr = get_next(ll, get_head(ll));
  int *foreward_order = malloc(sizeof(int) * THREAD_PUT_CNT * THREAD_CNT * 2);
  int i = 0;
  while (get_next(ll, ptr) != NULL) {
    foreward_order[i*2 + 0] = ptr->value.thread_num;
    foreward_order[i*2 + 1] = ptr->value.ctr;
    i++;
    // printf("thread %d, num %d\n", ptr->value[1], ptr->value[0]);
    ptr = get_next(ll, ptr);
  }
  printf("Running integrity test...\n");
  ptr = get_prev(ll, get_tail(ll));
  i--;
  while (get_prev(ll, ptr) != NULL) {
    if (ptr->value.thread_num != foreward_order[i*2 + 0] || ptr->value.ctr != foreward_order[i*2 + 1]) {
      printf("Newp\n");
    }
    i--;
    ptr = get_prev(ll, ptr);
  }
  printf("Final consitancy checks...\n");
  int consistancy_errors;
  int inconsistant_errors;
  consistancy_errors = 0;
  inconsistant_errors = 0;
  consistancy_check_forwards(ll, THREAD_CNT, &inconsistant_errors, &consistancy_errors);
  printf("Forewards: %d inconst, %d const\n", inconsistant_errors, consistancy_errors);
  consistancy_errors = 0;
  inconsistant_errors = 0;
  printf("%p %p\n", ll, get_tail(ll));
  consistancy_check_reverse(ll, THREAD_CNT, &inconsistant_errors, &consistancy_errors, last_inserted_arr);
  printf("Reverse: %d inconst, %d const\n", inconsistant_errors, consistancy_errors);
  return 0;
}
