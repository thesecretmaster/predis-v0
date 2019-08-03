#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "predis.h"

#include "types/int.h"
#include "types/string.h"

volatile __thread bool *safe = NULL;

#define DATA_TYPE_COUNT 2
static struct data_type data_types[DATA_TYPE_COUNT];

static int initEle(struct main_ele *me) {
  me->ptr = NULL;
  me->type = NULL;
  me->pending_delete = false;
  return 0;
}

int free_predis(struct main_struct *ms) {
  clean_queue(ms);
  for (int i = 0; i < ms->size; i++) {
    struct main_ele *ele = ms->elements + i;
    if (ele->type != NULL) {
      ele->type->free_ele(ele->ptr);
    }
  }
  struct thread_info_list *tiele = ms->thread_list;
  struct thread_info_list *prev;
  while (tiele != NULL) {
    prev = tiele;
    tiele = tiele->next;
    free(prev);
  }
  struct element_queue *fl_ele = ms->free_list;
  struct element_queue *prev_fl;
  while (fl_ele != NULL) {
    prev_fl = fl_ele;
    fl_ele = fl_ele->next;
    free(prev_fl);
  }
  free(ms->elements);
  free(ms);
  return 0;
}

struct main_struct* init(int size) {
  data_types[0] = data_type_int;
  data_types[1] = data_type_string;
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

struct thread_info_list* register_thread(struct main_struct *ms) {
  struct thread_info_list *ti_ele = malloc(sizeof(struct thread_info_list));
  ti_ele->safe = true;
  safe = &(ti_ele->safe);
  ti_ele->next = ms->thread_list;
  while (!__sync_bool_compare_and_swap(&(ms->thread_list), ti_ele->next, ti_ele)) {
    ti_ele->next = ms->thread_list;
  }
  return ti_ele;
}

// Errors:
// NULL: Not found
static struct data_type* getDataType(struct data_type* dt_list, int dt_max, char* dt_name) {
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
// Notes:
//  - The pointed passed in for dt_name or raw_val CANNOT be freed
//    until the element is deleted
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
      fl_head = *free_list;
      if (fl_head != NULL) {
        if (__sync_bool_compare_and_swap(free_list, fl_head, fl_head->next)) {
          val = fl_head->element;
          idx = fl_head->idx;
          free(fl_head);
          break;
        }
      }
      idx = __sync_fetch_and_add(&(ms->counter), 1);
      if (idx >= ms->size) {
        __sync_fetch_and_sub(&(ms->counter), 1);
      } else {
        val = ms->elements + idx;
        break;
      }
      if (fail_count > 10) {
        return -2;
      }
      clean_queue(ms);
      sched_yield();
      fail_count++;
    }
    val->pending_delete = false;
    val->type = dt;
    dt->setter(&(val->ptr), raw_val);
    return idx;
  }
}

// Errors:
// -1: Invalid data type
// -2: Pending deletion
// -3: Data types did not match
// -4: Index out of bounds
// -5: Element is not set
int get(char* dt_name, struct main_struct* ms, struct return_val* rval, int idx) {
  if (idx < 0 || idx > ms->size) { return -4; }
  __atomic_store_n(safe, false, __ATOMIC_SEQ_CST);
  struct main_ele *ele = ms->elements + idx;
  if (ele->type == NULL)   { return -5; }
  if (ele->pending_delete) { return -2; }
  int dt_max = DATA_TYPE_COUNT;
  struct data_type* dt_list = data_types;
  struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return -1;
  } else {
    if (strcmp(ele->type->name, dt_name) != 0) {
      return -3;
    }
    void *val = ele->ptr;
    int errors = dt->getter(val, rval);
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return errors;
  }
}

// Errors:
// -1: Invalid data type
// -2: Pending deletion
// -3: Wrong data type
// -4: Invalid updater name
int update(char* dt_name, char* updater_name, struct main_struct* ms, char* raw_new_val, int idx) {
  __atomic_store_n(safe, false, __ATOMIC_SEQ_CST);
  struct main_ele *ele = ms->elements + idx;
  int dt_max = DATA_TYPE_COUNT;
  struct data_type* dt_list = data_types;
  struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return -1;
  } else {
    if (strcmp(ele->type->name, dt_name) != 0) { return -3; }
    if (ele->pending_delete == true) { return -2; }
    const struct updater *updater = NULL;
    for (int i = 0; i < dt->updater_length; i++) {
      if (strcmp(dt->updaters[i].name, updater_name) == 0) {
        updater = dt->updaters + i;
        break;
      }
    }
    if (updater == NULL) { return -4; }
    int errors;
    if (updater->safe) {
      errors = (updater->func)(&(ele->ptr), raw_new_val);
    } else {
      void *val = ele->ptr;
      void *clone_val = dt->clone(val);
      errors = (updater->func)(&clone_val, raw_new_val);
      while (!__sync_bool_compare_and_swap(&(ele->ptr), val, clone_val)) {
        val = ele->ptr;
        clone_val = dt->clone(val);
        errors = (updater->func)(&clone_val, raw_new_val);
      }
    }
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return errors;
  }
}

// Errors:
// -1: Already deleted
int del(struct main_struct *ms, int idx) {
  struct main_ele *ele = ms->elements + idx;
  bool b = false;
  if (!__atomic_compare_exchange_n(&(ele->pending_delete), &b, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    return -1;
  }
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
  struct main_ele *ele;
  ele = delq_tail->element;
  ele->pending_delete = false;
  if (ele->type != NULL) {
    ele->type->free_ele(ele->ptr);
    ele->type = NULL;
    ele->ptr = NULL;
  }
  while (delq_tail->next != NULL) {
    delq_tail = delq_tail->next;
    ele = delq_tail->element;
    ele->pending_delete = false;
    if (ele->type != NULL) {
      ele->type->free_ele(ele->ptr);
      ele->type = NULL;
      ele->ptr = NULL;
    }
  }
  delq_tail->next = *free_list;
  while (!__sync_bool_compare_and_swap(free_list, delq_tail->next, delq_head)) {
    delq_tail->next = *free_list;
  }
  return 0;
}
