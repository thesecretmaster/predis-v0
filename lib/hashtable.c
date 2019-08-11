#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hashtable.h"

struct ht_table *ht_init(int size) {
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->elements = malloc(sizeof(struct ht_elem)*size);
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

// Errors:
// -1: Not found
int ht_find(struct ht_table *table, char *key) {
  struct ht_elem *elem = table->elements[ht_hash(key) % table->size];
  while (elem != NULL) {
    if (elem->skip_status == 0 && strcmp(elem->key, key) == 0) {
      return elem->value;
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
  struct ht_elem *elem = prev->next;
  if (elem == NULL && strcmp(prev->key, key) == 0) {
    if (!__atomic_compare_exchange_n((table->elements + (ht_hash(key) % table->size)), &prev, NULL, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      return -2;
    } else {
      // Append to the head of the free list
      return 0;
    }
  }
  while (elem != NULL) {
    if (strcmp(elem->key, key) == 0) {
      if (!__atomic_compare_exchange_n(&(prev->next), &elem, elem->next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return -2;
      } else {
        // Append to the head of the free list
        return 0;
      }
    }
    prev = elem;
    elem = elem->next;
  }
  return -1;
}
