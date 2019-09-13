#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hashtable.h"

struct ht_table *ht_init(int size) {
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->elements = malloc(sizeof(struct ht_elem)*size);
  table->free_list = NULL;
  // Could this loop be made faster by setting the first element and memcpy-ing?
  for (int i = 0; i < size; i++) {
    table->elements[i] = NULL;
  }
  table->size = size;
  return table;
}

void ht_free(struct ht_table *table) {
  struct ht_elem *elem;
  struct ht_elem *prev;
  for (int i = 0; i < table->size; i++) {
    elem = table->elements[i];
    while (elem != NULL) {
      prev = elem;
      elem = elem->next;
      free(prev->key);
      free(prev);
    }
  }
  struct ht_free_list *fl_elem;
  struct ht_free_list *fl_prev;
  fl_elem = table->free_list;
  while (fl_elem != NULL) {
    fl_prev = fl_elem;
    fl_elem = fl_elem->next;
    free(fl_prev->elem);
    free(fl_prev);
  }
  free(table->elements);
  free(table);
}

// https://stackoverflow.com/a/7666577/4948732
unsigned int ht_hash(char *str) {
  int hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

struct ht_free_list *ht_clean_prepare(struct ht_table *table) {
  struct ht_free_list *fl_head;
  void *nullptr = NULL;
  __atomic_exchange(&(table->free_list), &nullptr, &fl_head, __ATOMIC_SEQ_CST);
  return fl_head;
}

int ht_clean_run(struct ht_free_list *fl_head) {
  if (fl_head == NULL) { return -1; }
  struct ht_free_list *prev;
  while (fl_head != NULL) {
    free(fl_head->elem->key);
    free(fl_head->elem);
    prev = fl_head;
    fl_head = fl_head->next;
    free(prev);
  }
  return 0;
}

// Errors:
// -1: Not found
int ht_find(struct ht_table *table, char *key) {
  struct ht_elem *elem = table->elements[ht_hash(key) % table->size];
  int val;
  while (elem != NULL) {
    if (elem->skip_status == 0 && strcmp(elem->key, key) == 0) {
      val = elem->value;
      return val;
    }
    elem = elem->next;
  }
  return -1;
}

// Statuses:
// -1: Already existed
// -2: CAS failed, try again
int ht_store(struct ht_table *table, char *key, int val) {
  struct ht_elem *elem = table->elements[ht_hash(key) % table->size];
  struct ht_elem *new_elem = malloc(sizeof(struct ht_elem));
  new_elem->key = strdup(key);
  new_elem->value = val;
  new_elem->skip_status = 1;
  new_elem->next = elem;
  if (!__atomic_compare_exchange_n(table->elements + (ht_hash(key) % table->size), &elem, new_elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    free(new_elem);
    return -2;
  }
  while (elem != NULL) {
    if (strcmp(elem->key, key) == 0) {
      new_elem->skip_status = 2;
      return -1;
    }
    elem = elem->next;
  }
  new_elem->skip_status = 0;
  return 0;
}

// -1: Did not exist
// -2: CAS failed. Try again.
int ht_delete(struct ht_table *table, char *key) {
  struct ht_elem *prev = table->elements[ht_hash(key) % table->size];
  if (prev == NULL) {
    return -1;
  }
  struct ht_free_list *fl_new_node;
  struct ht_elem *elem = prev->next;
  if (elem == NULL && strcmp(prev->key, key) == 0) {
    if (!__atomic_compare_exchange_n((table->elements + (ht_hash(key) % table->size)), &prev, NULL, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      return -2;
    } else {
      fl_new_node = malloc(sizeof(struct ht_free_list));
      fl_new_node->elem = prev;
      fl_new_node->next = table->free_list;
      while (!__atomic_compare_exchange_n(&(table->free_list), &(fl_new_node->next), fl_new_node, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        fl_new_node->next = table->free_list;
      }
      return 0;
    }
  }
  while (elem != NULL) {
    if (strcmp(elem->key, key) == 0) {
      if (!__atomic_compare_exchange_n(&(prev->next), &elem, elem->next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return -2;
      } else {
        fl_new_node = malloc(sizeof(struct ht_free_list));
        fl_new_node->elem = elem;
        fl_new_node->next = table->free_list;
        while (!__atomic_compare_exchange_n(&(table->free_list), &(fl_new_node->next), fl_new_node, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
          fl_new_node->next = table->free_list;
        }
        return 0;
      }
    }
    prev = elem;
    elem = elem->next;
  }
  return -1;
}
