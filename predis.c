#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "predis.h" // This also includes the hashtable

#include "types/type_ll.h"
#define GEN_DT_LL
#include "types/int.h"
#include "types/string.h"
#include "types/linked_list.h"
#undef GEN_DT_LL
#include "dt_hash.c"

#define HT_ALLOC_INCR 65536 // 2^16

volatile __thread bool *safe = NULL;

static const struct data_type **data_types = NULL;
static int data_type_count = 0;
#define DATA_TYPE_COUNT data_type_count
// #define DATA_TYPE_COUNT 2
// static struct data_type data_types[DATA_TYPE_COUNT];

static int initEle(struct main_ele *me) {
  me->ptr = NULL;
  me->type = NULL;
  me->pending_delete = false;
  return 0;
}

int free_predis(struct main_struct *ms) {
  clean_queue(ms);
  ht_free(ms->hashtable);
  // for (int i = 0; i < ms->size; i++) {
  //   struct main_ele *ele = ms->elements + i;
  //   if (ele->type != NULL) {
  //     ele->type->free_ele(ele->ptr);
  //   }
  // }
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
  // free(ms->elements);
  free(ms);
  return 0;
}

struct main_struct* init(int size) {
  int dt_count = MAX_HASH_VALUE - MIN_HASH_VALUE + 1;
  data_types = malloc(sizeof(struct data_type*)*dt_count);
  struct data_type_ll *dt_ll = &PREV_DT_LL;
  while (dt_ll->prev != NULL) {
    char *dt_name = dt_ll->type->name;
    int hsh = dt_hash(dt_name, strlen(dt_name)) - MIN_HASH_VALUE;
    data_types[hsh] = dt_ll->type;
    dt_ll = dt_ll->prev;
  }

  // struct data_type_ll *dt_ll = &PREV_DT_LL;
  // while (dt_ll->prev != NULL) {
  //   data_type_count++;
  //   dt_ll = dt_ll->prev;
  // }
  // data_types = malloc(sizeof(struct data_type*)*data_type_count);
  // dt_ll = &PREV_DT_LL;
  // int ctr = 0;
  // while (dt_ll->prev != NULL) {
  //   data_types[ctr] = dt_ll->type;
  //   ctr++;
  //   dt_ll = dt_ll->prev;
  // }

  // data_types[0] = data_type_int;
  // data_types[1] = data_type_string;
  struct main_struct *ms = malloc(sizeof(struct main_struct));
  ms->hashtable = ht_init(size, &initEle);
  ms->deletion_queue = NULL;
  ms->free_list = NULL;
  ms->thread_list = NULL;
  ms->thread_list_write_locked = false;
  ms->thread_list_traversing_count = 0;
  ms->allocation_incr = HT_ALLOC_INCR;
  ms->allocation_idx = 0;
  ms->allocation = malloc(sizeof(struct main_ele)*ms->allocation_incr);
  return ms;
}

struct thread_info_list* register_thread(struct main_struct *ms) {
  struct thread_info_list *ti_ele = malloc(sizeof(struct thread_info_list));
  ti_ele->safe = true;
  safe = &(ti_ele->safe); // Set the global variable
  ti_ele->next = ms->thread_list;
  while (!__sync_bool_compare_and_swap(&(ms->thread_list), ti_ele->next, ti_ele)) {
    ti_ele->next = ms->thread_list;
  }
  return ti_ele;
}

// In theory this could be simpler. See https://cs.stackexchange.com/questions/112609/parallel-deletion-and-traversal-in-a-lock-free-linked-list
void deregister_thread(struct main_struct *ms, struct thread_info_list *ti) {
  bool falsevar = false;
  // Grab the write lock
  while (!__atomic_compare_exchange_n(&(ms->thread_list_write_locked), &falsevar, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
  // Wait until all the traversers leave
  while (__atomic_load_n(&(ms->thread_list_traversing_count), __ATOMIC_SEQ_CST) != 0) {}
  // Now we have total control! Mwa ha ha ha ha!

  // Get a pointer to the previous element (tiptr)
  struct thread_info_list *tiptr = ms->thread_list;
  if (ti  == tiptr) {
    ms->thread_list = ti->next;
  } else {
    while (tiptr->next != ti) {
      tiptr = tiptr->next;
      // This should never happen, but it did so...?
      if (tiptr == NULL) {
        __atomic_store_n(&(ms->thread_list_write_locked), false, __ATOMIC_SEQ_CST);
        return;
      }
    }
    // Delete ti
    tiptr->next = ti->next;
  }

  // Release the lock
  __atomic_store_n(&(ms->thread_list_write_locked), false, __ATOMIC_SEQ_CST);

  // Clean up
  free(ti);
  return;
}

static const struct updater *get_updater(const struct data_type *dt, char *name){
  for (int i = 0; i < dt->updater_length; i++)
    if (strcmp(dt->updaters[i]->name, name) == 0)
      return dt->updaters[i];
  return NULL;
}

static const struct getter *get_getter(const struct data_type *dt, char *name){
  for (int i = 0; i < dt->getter_length; i++)
    if (strcmp(dt->getters[i]->name, name) == 0)
      return dt->getters[i];
  return NULL;
}

// Errors:
// NULL: Not found
static const struct data_type* getDataType(const struct data_type** dt_list, int dt_max, char* dt_name) {
  int dt_name_len = strlen(dt_name);
  if (dt_valid(dt_name, dt_name_len)) {
    return data_types[dt_hash(dt_name, dt_name_len) - MIN_HASH_VALUE];
  } else {
    return NULL;
  }
  // int i = 0;
  // while (i < dt_max) {
  //   if (strcmp(dt_list[i]->name, dt_name) == 0) {
  //     return dt_list[i];
  //   }
  //   i++;
  // }
  // return NULL;
}

// Errors:
int set(char* dt_name, struct main_struct* ms, char* key, char **args) {
  int dt_max = DATA_TYPE_COUNT;
  const struct data_type** dt_list = data_types;
  const struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    return -1;
  } else {
    volatile struct main_ele *val;
    struct element_queue *fl_head;
    // Concept: Keep a fl_length field, and if it's longer than liek 5, try popping off it
    do {
      fl_head = ms->free_list;
    } while (fl_head != NULL && !__atomic_compare_exchange_n(&(ms->free_list), &(fl_head), fl_head->next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    if (fl_head != NULL) {
      val = fl_head->element;
    } else {
      // If idx == SIZE_MAX
      //   make a new allocation
      //   reset idx
      // If idx > SIZE_MAX
      //   while (idx > SIZE_MAX) { idx = idx + 1}
      int idx = __atomic_fetch_add(&(ms->allocation_idx), 1, __ATOMIC_SEQ_CST);
      if (idx == ms->allocation_incr) {
        __atomic_store_n(&(ms->allocation), malloc(sizeof(struct main_ele)*ms->allocation_incr), __ATOMIC_SEQ_CST);
        idx = 0;
        __atomic_store_n(&(ms->allocation_idx), 1, __ATOMIC_SEQ_CST);
      } else if (idx > ms->allocation_incr) {
        while (idx > ms->allocation_idx) {
          idx = __atomic_fetch_add(&(ms->allocation_idx), 1, __ATOMIC_SEQ_CST);
        }
      }
      val = ms->allocation + (idx % ms->allocation_incr);
    }
    val->pending_delete = false;
    val->type = dt;
    int errors = 0;
    val->ptr = dt->initializer(&errors, args);
    if (errors != 0) {
      // Cleanup val and stuff
      return errors;
    } else {
      ht_store(ms->hashtable, key, (struct main_ele*)val);
      return 0;
    }
  }
}

// Errors:
// -1: Invalid data type
// -2: Pending deletion
// -3: Data types did not match
// -4: Index out of bounds
// -5: Element is not set
int get(char* dt_name, struct main_struct* ms, char *getter_name, struct return_val* rval, char *key, char **args) {
  // if (idx < 0 || idx > ms->size) { return -4; }
  if (key == NULL) { return -6; }
  __atomic_store_n(safe, false, __ATOMIC_SEQ_CST);
  // struct main_ele *ele = ms->elements + idx;
  struct main_ele *ele = ht_find(ms->hashtable, key);
  if (ele == NULL) { return -6; }
  if (ele->type == NULL)   { return -5; }
  if (ele->pending_delete) { return -2; }
  int dt_max = DATA_TYPE_COUNT;
  const struct data_type** dt_list = data_types;
  const struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return -1;
  } else {
    const struct getter *getter = get_getter(dt, getter_name);
    if (getter == NULL) {
      __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
      return -3;
    }
    int errors = getter->func(rval, ele->ptr, args);
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return errors;
  }
}

// Errors:
// -1: Invalid data type
// -2: Pending deletion
// -4: Invalid updater name
// -5: Invalid key
int update(char* dt_name, char* updater_name, struct main_struct* ms, char* key, char **args) {
  __atomic_store_n(safe, false, __ATOMIC_SEQ_CST);
  // struct main_ele *ele = ms->elements + idx;
  struct main_ele *ele = ht_find(ms->hashtable, key);
  if (ele == NULL) { return -5; }
  int dt_max = DATA_TYPE_COUNT;
  const struct data_type** dt_list = data_types;
  const struct data_type *dt = getDataType(dt_list, dt_max, dt_name);
  if (dt == NULL) {
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return -1;
  } else {
    if (ele->pending_delete == true) { return -2; }
    const struct updater *updater = get_updater(dt, updater_name);
    if (updater == NULL) { return -4; }
    int errors;
    if (updater->safe) {
      errors = (updater->func)(&(ele->ptr), args);
    } else {
      void *val = ele->ptr;
      void *clone_val = dt->clone(val);
      errors = (updater->func)(&clone_val, args);
      while (!__sync_bool_compare_and_swap(&(ele->ptr), val, clone_val)) {
        val = ele->ptr;
        clone_val = dt->clone(val);
        errors = (updater->func)(&clone_val, args);
      }
    }
    __atomic_store_n(safe, true, __ATOMIC_SEQ_CST);
    return errors;
  }
}

// Errors:
// -1: Already deleted
int del(struct main_struct *ms, char *key) {
  struct main_ele *ele = ht_find(ms->hashtable, key);
  if (ele == NULL) { return -1; }
  bool b = false;
  if (!__atomic_compare_exchange_n(&(ele->pending_delete), &b, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    return -1;
  }
  struct element_queue **queue = &(ms->deletion_queue);
  struct element_queue *dq_ele = malloc(sizeof(struct element_queue));
  dq_ele->element = ele;
  dq_ele->next = *queue;
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
  struct ht_free_list *ht_preped = ht_clean_prepare(ms->hashtable);
  // Before we use tilist, we need to wait for any writers to be done and increment the semaphor
  // This is like a wacky custom version of auto-retry transactions
  while (1) {
    while (__atomic_load_n(&(ms->thread_list_write_locked), __ATOMIC_SEQ_CST) == true) {}
    // There's a small risk that right here, somebody else grabbed the write lock
    // So first, we pretend that it didn't happen:
    __atomic_add_fetch(&(ms->thread_list_traversing_count), 1, __ATOMIC_SEQ_CST);
    // But then if it did happen, we revert our changes and try again (otherwise we just proceed)
    if (__atomic_load_n(&(ms->thread_list_write_locked), __ATOMIC_SEQ_CST) == false) {
      break;
    } else {
      __atomic_sub_fetch(&(ms->thread_list_traversing_count), 1, __ATOMIC_SEQ_CST);
    }
  }
  while (tilist != NULL) {
    while (!(tilist->safe)) {}
    tilist = tilist->next;
  }
  __atomic_sub_fetch(&(ms->thread_list_traversing_count), 1, __ATOMIC_SEQ_CST);
  ht_clean_run(ht_preped);
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
