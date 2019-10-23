#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct ht_bucket {
  union ht_bucket_elem *elements;
};

// We do a very sketchy thing here, where we store a flag in the lowest
// bit of the pointer. 1 means it's a bucket, 0 means it's an element.
// NOTE: It is important the bucket == 1 and elem == 0 because we need
// to do NULL checks against elems.
union ht_bucket_elem {
  struct ht_elem* elem;
  struct ht_bucket* bucket;
};
bool ht_is_bucket(union ht_bucket_elem e) { return (uintptr_t)e.bucket & 0x1; }
union ht_bucket_elem ht_set_bucket(struct ht_bucket* bucket) { return (union ht_bucket_elem)(struct ht_bucket*)((uintptr_t)bucket | 0x1); }
union ht_bucket_elem ht_set_elem(struct ht_elem* elem) { return (union ht_bucket_elem)(struct ht_elem*)((uintptr_t)elem & (~0x1)); }

// https://stackoverflow.com/a/7666577/4948732
unsigned int ht_hash(char *str) {
  int hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

struct ht_bucket *ht_bucket_init(struct ht_table *table) {
  struct ht_bucket *newbucket = malloc(sizeof(struct ht_bucket));
  int element_len = 0x1 << table->bitlen;
  newbucket->elements = malloc(sizeof(union ht_bucket_elem) * element_len);
  for (int i = 0; i < element_len; i++) {
    newbucket->elements[i].elem = NULL;
  }
  return newbucket;
}

struct ht_table *ht_init(int bitlen) {
  bitlen = 8;
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->bitlen = bitlen;
  table->free_list = NULL;
  table->root_bucket = ht_bucket_init(table);
  return table;
}

struct ht_elem **ht_elem_internal(struct ht_table *table, char *key, bool createbucket) {
  unsigned int hsh = ht_hash(key);
  unsigned int level = 0;
  unsigned int idx_bitmask = 0x0;
  for (int i = 0; i < table->bitlen; i++) {
    idx_bitmask = idx_bitmask << 1;
    idx_bitmask = idx_bitmask | 0x1;
  }
  unsigned int idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
  struct ht_bucket *curr_bucket = table->root_bucket;
  while (curr_bucket->elements[idx].elem != NULL && ht_is_bucket(curr_bucket->elements[idx])) {
    curr_bucket = curr_bucket->elements[idx].bucket;
    level++;
    idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
  }
  if (createbucket && curr_bucket->elements[idx].elem != NULL && ((level + 1) * table->bitlen) < sizeof(union ht_bucket_elem)) {
    curr_bucket->elements[idx] = ht_set_bucket(ht_bucket_init(table));
    curr_bucket = curr_bucket->elements[idx].bucket;
    idx = (hsh >> ((level + 1) * table->bitlen)) & idx_bitmask;
  }
  return &(curr_bucket->elements[idx].elem);
}

int ht_find(struct ht_table *table, char *key) {
  struct ht_elem *elem = *ht_elem_internal(table, key, false);
  if (elem == NULL) {
    return -1;
  } else {
    unsigned int key_hash = ht_hash(key);
    // This is just while (key != elem->key) but with speed improvements
    while (!(key_hash == elem->key_hash && strcmp(key, elem->key) == 0)) {
      elem = elem->next;
      if (elem == NULL) { return -1; }
    }
    return elem->value;
  }
}

int ht_store(struct ht_table *table, char *key, int value) {
  struct ht_elem **elem = ht_elem_internal(table, key, false);
  struct ht_elem *nn_elem = *elem;
  struct ht_elem *prev = NULL;
  unsigned int key_hash = ht_hash(key);
  // The second clause here is just saying nn_elem->key != key, but with speed improvments
  while (nn_elem != NULL && !(key_hash == nn_elem->key_hash && strcmp(nn_elem->key, key) == 0)) {
    prev = nn_elem;
    nn_elem = nn_elem->next;
  }
  if (nn_elem != NULL) {
    nn_elem->value = value;
    return 0;
  } else {
    struct ht_elem *new_elem = malloc(sizeof(struct ht_elem));
    new_elem->key = strdup(key);
    new_elem->key_hash = ht_hash(key);
    new_elem->value = value;
    new_elem->next = NULL;
    if (prev == NULL) {
      *elem = new_elem;
    } else {
      prev->next = new_elem;
    }
    return 0;
  }
}

int ht_delete(struct ht_table *table, char *key) {
  struct ht_elem **elem = ht_elem_internal(table, key, false);
  struct ht_elem *nn_elem = *elem;
  struct ht_elem *prev = NULL;
  unsigned int key_hash = ht_hash(key);
  // The second clause is just saying key != nn_elem->key, but with speed improvments
  while (nn_elem != NULL && !(key_hash == nn_elem->key_hash && strcmp(key, nn_elem->key) == 0)) {
    prev = nn_elem;
    nn_elem = nn_elem->next;
  }
  if (nn_elem == NULL) {
    return -1;
  } else {
    if (prev == NULL) {
      *elem = nn_elem->next;
      free(nn_elem);
    } else {
      prev->next = nn_elem->next;
    }
    return 0;
  }
}

void ht_free(struct ht_table *table) { return;}

struct ht_free_list *ht_clean_prepare(struct ht_table *table) {return NULL;}
int ht_clean_run(struct ht_free_list *fl) { return 0; }
