#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "template.h"
#include "linked_list.h"

static int push(void**, char**);
const static struct updater pusher = {
  .name = "push",
  .func = &push,
  .safe = true
};

static int pop(struct return_val*, void*, char**);
const static struct getter poper = {
  .name = "pop",
  .func = &pop,
  .safe = true
};

static int get(struct return_val*, void*, char**);
const static struct getter fetch = {
  .name = "fetch",
  .func = &get,
  .safe = true
};

static void *set(int *errors, char **args);

const static struct getter *getters[] = {&poper, &fetch};
const static struct updater *updaters[] = {&pusher};

const struct data_type data_type_linked_list = {
  .name = "linked_list",
  .initializer_safe = true,
  .getter_length = 2,
  .getters = getters,
  .initializer = &set,
  .free_ele = &free_ele,
  .updater_length = 1,
  .updaters = updaters,
  .clone = &clone
};

struct linked_list_elem {
  volatile char *value;
  volatile int lock;
  volatile struct linked_list_elem *next;
  volatile struct linked_list_elem *prev;
};

struct linked_list {
  volatile int lock;
  volatile long int length;
  volatile struct linked_list_elem *head;
  volatile struct linked_list_elem *tail;
};

static int free_ele(void *val) {
  free(val);
  return 0;
}

static int get(struct return_val *rval, void *val, char **args) {
  long int idx = strtol(args[0], NULL, 10);
  struct linked_list *ll = val;
  volatile struct linked_list_elem *elem = ll->head;
  if (idx < 0 || elem == NULL) {
    return -1;
  }
  while (idx > 0 && elem != NULL) {
    elem = elem->next;
    idx--;
  }
  if (idx > 0 || elem == NULL) {
    return -1;
  }
  rval->value = (char*)elem->value;
  return 0;
}

static int pop(struct return_val *rval, void *val, char **args) {
  volatile struct linked_list *ll = val;
  volatile struct linked_list_elem *head;
  while (!__atomic_test_and_set(&(ll->lock), __ATOMIC_SEQ_CST)) {};
  head = ll->head;
  if (head == NULL) {
    __atomic_clear(&(ll->lock), __ATOMIC_SEQ_CST);
    return -1;
  } else {
    // Don't worry about releasing this lock because we trash the element :P
    while (!__atomic_test_and_set(&(head->lock), __ATOMIC_SEQ_CST)) {};
    if (head->next != NULL) {
      while (!__atomic_test_and_set(&(head->next->lock), __ATOMIC_SEQ_CST)) {};
      head->next->prev = NULL;
      __atomic_clear(&(head->next->lock), __ATOMIC_SEQ_CST);
    }
    __atomic_add_fetch(&(ll->length), -1, __ATOMIC_SEQ_CST);
    ll->head = head->next;
    __atomic_clear(&(ll->lock), __ATOMIC_SEQ_CST);
    rval->value = (char*)head->value;
    return 0;
  }
}

static void *set(int *errors, char **_args) {
  struct linked_list *ll = malloc(sizeof(struct linked_list));
  ll->head = NULL;
  __atomic_clear(&(ll->lock), __ATOMIC_SEQ_CST);
  ll->tail = NULL;
  return ll;
}

static int push(void **oldval, char **args) {
  volatile struct linked_list_elem *head = NULL;
  volatile struct linked_list *ll = *oldval;
  volatile struct linked_list_elem *elem = malloc(sizeof(struct linked_list_elem));
  elem->value = strdup(args[0]);
  elem->prev = NULL;
  __atomic_clear(&(elem->lock), __ATOMIC_SEQ_CST);
  while (!__atomic_test_and_set(&(ll->lock), __ATOMIC_SEQ_CST)) {};
  head = ll->head;
  if (head != NULL) {
    while (!__atomic_test_and_set(&(head->lock), __ATOMIC_SEQ_CST)) {};
    head->prev = elem;
  }
  __atomic_add_fetch(&(ll->length), 1, __ATOMIC_SEQ_CST);
  elem->next = ll->head;
  ll->head = elem;
  if (head != NULL) {
    __atomic_clear(&(head->lock), __ATOMIC_SEQ_CST);
  }
  __atomic_clear(&(ll->lock), __ATOMIC_SEQ_CST);
  return 0;
}

static void *clone(void *oldval) {
  return strdup((char*)oldval);
}

static int update(void **oldval, char **args) {
  char *tmp = *oldval;
  *oldval = strdup(args[0]);
  free(tmp);
  return 0;
}
