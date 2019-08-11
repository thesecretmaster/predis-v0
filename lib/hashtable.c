#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hashtable.h"

struct ht_table *ht_init(int size) {
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->elements = malloc(sizeof(struct ht_elem)*size);
  table->write_locked = false;
  table->reader_count = 0;
  // Could this loop be made faster by setting the first element and memcpy-ing?
  for (int i = 0; i < size; i++) {
    table->elements[i] = NULL;
  }
  table->size = size;
  return table;
}

int ht_hash(char *key) {
  return key[0];
}

int ht_clean(struct ht_table *table) {
  bool falsevar = false;
  while (!__atomic_compare_exchange_n(&(table->write_locked), &falsevar, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
  while (__atomic_load_n(&(table->reader_count), __ATOMIC_SEQ_CST) != 0) {}
  struct ht_free_list *fl_head = table->free_list;
  struct ht_free_list *prev;
  while (fl_head != NULL) {
    prev = fl_head;
    fl_head = fl_head->next;
    free(prev->elem);
    free(prev);
  }
  __atomic_store_n(&(table->write_locked), false, __ATOMIC_SEQ_CST);
  return 0;
}

// Errors:
// -1: Not found
int ht_find(struct ht_table *table, char *key) {
  __atomic_add_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
  while (__atomic_load_n(&(table->write_locked), __ATOMIC_SEQ_CST) == true) {}

  struct ht_elem *elem = table->elements[ht_hash(key) % table->size];
  int val;
  while (elem != NULL) {
    if (elem->skip_status == 0 && strcmp(elem->key, key) == 0) {
      val = elem->value;
      __atomic_sub_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
      return val;
    }
    elem = elem->next;
  }
  __atomic_sub_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
  return -1;
}

// Statuses:
// -1: Already existed
// -2: CAS failed, try again
int ht_store(struct ht_table *table, char *key, int val) {
  __atomic_add_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
  while (__atomic_load_n(&(table->write_locked), __ATOMIC_SEQ_CST) == true) {}

  struct ht_elem *elem = table->elements[ht_hash(key) % table->size];
  struct ht_elem *new_elem = malloc(sizeof(struct ht_elem));
  new_elem->key = strdup(key);
  new_elem->value = val;
  new_elem->skip_status = 1;
  new_elem->next = elem;
  if (!__atomic_compare_exchange_n(table->elements + (ht_hash(key) % table->size), &elem, new_elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    free(new_elem);
    __atomic_sub_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
    return -2;
  }
  while (elem != NULL) {
    if (strcmp(elem->key, key) == 0) {
      new_elem->skip_status = 2;
      __atomic_sub_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
      return -1;
    }
    elem = elem->next;
  }
  new_elem->skip_status = 0;
  __atomic_sub_fetch(&(table->reader_count), 1, __ATOMIC_SEQ_CST);
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
