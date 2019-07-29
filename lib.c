#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "data_type.h"

#include "types/int.h"

#define DATA_TYPE_COUNT 1
struct data_type data_types[DATA_TYPE_COUNT];
volatile __thread bool *safe = NULL;

struct main_ele {
  char* type;
  void* ptr;
  bool pending_delete;
};

struct thread_info_list {
  struct thread_info_list *next;
  volatile bool safe;
};

struct element_queue {
  struct element_queue *next;
  struct main_ele *element;
  int idx;
};

struct main_struct {
  int size;
  int counter;
  struct main_ele *elements;
  struct element_queue *deletion_queue;
  struct element_queue *free_list;
  struct thread_info_list *thread_list;
};

int initEle(struct main_ele *me) {
  me->ptr = NULL;
  me->type = NULL;
  me->pending_delete = false;
  return 0;
}

int clean_queue(struct main_struct*);

struct main_struct* init(int size) {
  struct main_struct *ms = malloc(sizeof(struct main_struct));
  ms->size = size;
  ms->deletion_queue = NULL;
  ms->free_list = NULL;
  ms->thread_list = NULL;
  ms->counter = 0;
  ms->elements = malloc((ms->size)*sizeof(struct main_ele));
  struct main_ele *me_ptr = ms->elements;
  int rval;
  void* ub = ms->elements + ms->size;
  while ((void*)me_ptr < ub) {
    rval = initEle(me_ptr);
    if (rval != 0) {
      exit(rval);
    }
    me_ptr++;
  }
  return ms;
}

// Errors:
// NULL: Not found
struct data_type* getDataType(struct data_type* dt_list, int dt_max, char* dt_name) {
  int i = 0;
  while (i < dt_max) {
    if (strcmp(dt_list[i].name, dt_name) == 0) {
      return &(dt_list[i]);
    }
    i++;
  }
  return NULL;
}

// Errors:
// -1: Invalid data type
// -2: Out of space
int set(char* dt_name, struct main_struct* ms, char* raw_val) {
  int dt_max = DATA_TYPE_COUNT;
  struct data_type* dt_list = data_types;
  struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    return -1;
  } else {
    struct main_ele *val = NULL;
    struct element_queue **free_list = &(ms->free_list);
    struct element_queue *fl_head;
    int fail_count = 0;
    int idx;
    while (1) {
      idx = __sync_fetch_and_add(&(ms->counter), 1);
      if (idx >= ms->size) {
        __sync_fetch_and_sub(&(ms->counter), 1);
      } else {
        val = ms->elements + idx;
        break;
      }
      fl_head = *free_list;
      if (fl_head != NULL) {
        if (__sync_bool_compare_and_swap(free_list, fl_head, fl_head->next)) {
          // printf("Good cas!\n");
          val = fl_head->element;
          idx = fl_head->idx;
          free(fl_head);
          break;
        }
      }
      if (fail_count > 10) {
        return -2;
      }
      clean_queue(ms);
      sched_yield();
      fail_count++;
    }
    val->pending_delete = false;
    val->type = dt_name;
    dt->setter(&(val->ptr), raw_val);
    return idx;
  }
}

// Errors:
// -1: Invalid data type
// -2: Pending deletion
// -3: Data types did not match
int get(char* dt_name, struct main_struct* ms, struct return_val* rval, int idx) {
  *safe = false;
  struct main_ele *ele = ms->elements + idx;
  if (ele->pending_delete) {
    return -2;
  }
  if (strcmp(ele->type, dt_name) != 0) {
    return -3;
  }
  int dt_max = DATA_TYPE_COUNT;
  struct data_type* dt_list = data_types;
  struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    *safe = true;
    return -1;
  } else {
    if (ele->pending_delete == true) { return -2; }
    void *val = ele->ptr;
    int errors = dt->getter(val, rval);
    *safe = true;
    return errors;
  }
}

int update(char* dt_name, struct main_struct* ms, char* raw_new_val, int idx) {
  *safe = false;
  struct main_ele *ele = ms->elements + idx;
  if (strcmp(ele->type, dt_name) != 0) {
    return -3;
  }
  int dt_max = DATA_TYPE_COUNT;
  struct data_type* dt_list = data_types;
  struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    *safe = true;
    return -1;
  } else {
    if (ele->pending_delete == true) { return -2; }
    void *val = ele->ptr;
    int errors = dt->updater(val, raw_new_val);
    *safe = true;
    return errors;
  }
}

// Setters will use CAS for safety, which the data type can override
// Deletion just sets the lock bit THEN the deleted bit, it won't
// actually be freed until the next access (or in a background cleanup job)
int del(struct main_struct *ms, int idx) {
  struct main_ele *ele = ms->elements + idx;
  ele->pending_delete = true;
  struct element_queue **queue = &(ms->deletion_queue);
  struct element_queue *dq_ele = malloc(sizeof(struct element_queue));
  dq_ele->element = ele;
  dq_ele->next = *queue;
  dq_ele->idx = idx;
  while (!__sync_bool_compare_and_swap(queue, dq_ele->next, dq_ele)) {
    dq_ele->next = *queue;
  }
  return 0;
}

// Errors:
// -1: Nothing to do, delq already empty
int clean_queue(struct main_struct *ms) {
  struct element_queue **delq = &(ms->deletion_queue);
  struct thread_info_list *tilist = ms->thread_list;
  struct element_queue **free_list = &(ms->free_list);
  struct element_queue *delq_head = *delq;
  while (!__sync_bool_compare_and_swap(delq, delq_head, NULL)) {
    delq_head = *delq;
  }
  if (delq_head == NULL) {
    return -1;
  }
  while (tilist != NULL) {
    while (!(tilist->safe)) {}
    tilist = tilist->next;
  }
  struct element_queue *delq_tail = delq_head;
  while (delq_tail->next != NULL) {delq_tail = delq_tail->next;}
  delq_tail->next = *free_list;
  while (!__sync_bool_compare_and_swap(free_list, delq_tail->next, delq_head)) {
    delq_tail->next = *free_list;
  }
  return 0;
}
