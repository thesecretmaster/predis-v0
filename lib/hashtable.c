#include "hashtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct ht_bucket {
  volatile union ht_bucket_elem *elements;
};

// We do a very sketchy thing here, where we store a flag in the lowest
// bit of the pointer. 1 means it's a bucket, 0 means it's an element.
// NOTE: It is important the bucket == 1 and elem == 0 because we need
// to do NULL checks against elems.
union ht_bucket_elem {
  struct ht_elem* elem;
  struct ht_bucket* bucket;
};
bool ht_is_bucket(union ht_bucket_elem e) {
  return ((uintptr_t)e.bucket & 0x1) == 0x1;
}
union ht_bucket_elem ht_set_bucket(struct ht_bucket* bucket) {
  return (union ht_bucket_elem)(struct ht_bucket*)((uintptr_t)bucket | 0x1);
}
union ht_bucket_elem ht_set_element(struct ht_elem* elem) {
  return (union ht_bucket_elem)(struct ht_elem*)((uintptr_t)elem & (~0x1));
}
struct ht_bucket *ht_get_bucket(union ht_bucket_elem be) {
  assert(ht_is_bucket(be));
  return (struct ht_bucket*)((uintptr_t)be.bucket & (~0x1));
}

static struct ht_bucket *ht_bucket_init(struct ht_table *table) {
  struct ht_bucket *bucket = malloc(sizeof(struct ht_bucket));
  int element_count = (0x1 << table->bitlen);
  bucket->elements = malloc(sizeof(union ht_bucket_elem) * element_count);
  for (int i = 0; i < element_count; i++) {
    bucket->elements[i].elem = NULL;
  }
  return bucket;
}

// https://stackoverflow.com/a/7666577/4948732
const unsigned int ht_hash(const char *str) {
  int hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

struct ht_table *ht_init(int foo) {
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->bitlen = 8;
  table->free_list = NULL;
  table->root_bucket = ht_bucket_init(table);
  return table;
}

int ht_store(struct ht_table *table, const char *key, HT_VAL_TYPE *value) {
  // We use the key hash a lot, let's just do it once
  unsigned int key_hash = ht_hash(key);

  // Initialize new element
  struct ht_elem *new_element = malloc(sizeof(struct ht_elem));
  new_element->key_hash = key_hash;
  new_element->key = strdup(key);
  new_element->value = value;
  new_element->next = NULL;

  // Walk down the tree assuming that buckets aren't modified after our first fetch
  int depth = 0;
  unsigned int index_mask = ~(~0x0 << table->bitlen);
  unsigned int index = (key_hash >> (table->bitlen * depth)) & index_mask;
  struct ht_bucket *bucket = table->root_bucket;
  union ht_bucket_elem element = bucket->elements[index];
  while (ht_is_bucket(element)) {
    depth++;
    bucket = ht_get_bucket(element);
    index = (key_hash >> (table->bitlen * depth)) & index_mask;
    element = bucket->elements[index];
  }

  // Now our unchanged-bucket assumption is challenged, we'll pick up a
  // read-copy-update method where we do all of our work on the value of
  // element we've read, then we'll attempt to compare and swap our updates in.
  struct ht_bucket *new_bucket;
  // The next two can be uninitialized, it just makes gcc angry
  unsigned int old_element_index = 0;
  unsigned int new_element_index = 0;
  union ht_bucket_elem cas_target;
  bool new_bucket_created;
  do {
    new_bucket_created = false;
    element = bucket->elements[index];
    if (ht_is_bucket(element)) {
      // Our assumption has failed, let's try again from scratch.
      return ht_store(table, key, value);
    } else if (element.elem != NULL && element.elem->key_hash != key_hash) {
      // Allocate a new bucket.
      new_bucket_created = true;
      old_element_index = (element.elem->key_hash >> (table->bitlen * (depth + 1))) & index_mask;
      new_element_index = (key_hash               >> (table->bitlen * (depth + 1))) & index_mask;
      new_bucket = ht_bucket_init(table);
      // The order of the inserts is important in case old_element_index == new_element_index
      new_bucket->elements[new_element_index].elem = new_element;
      new_bucket->elements[old_element_index] = element;
      cas_target = ht_set_bucket(new_bucket);
    } else {
      new_element->next = element.elem;
      cas_target.elem = new_element;
      // Simple store, the best case :)
    }
  } while (!__atomic_compare_exchange(&(bucket->elements[index]), &element, &cas_target, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  if (new_bucket_created && old_element_index == new_element_index) {
    return ht_store(table, key, value);
  }
  return 0;
}

HT_VAL_TYPE* ht_find(struct ht_table *table, const char *key) {
  unsigned int key_hash = ht_hash(key);

  // Walk down the tree assuming that buckets aren't modified after our first fetch
  int depth = 0;
  unsigned int index_mask = ~(~0x0 << table->bitlen);
  unsigned int index = (key_hash >> (table->bitlen * depth)) & index_mask;
  struct ht_bucket *bucket = table->root_bucket;
  union ht_bucket_elem element = bucket->elements[index];
  while (ht_is_bucket(element)) {
    bucket = ht_get_bucket(element);
    depth++;
    index = (key_hash >> (table->bitlen * depth)) & index_mask;
    element = bucket->elements[index];
  }

  // Alternate code for dupe checking
  // union ht_bucket_elem felement;
  // felement.elem = NULL;
  // int elem_ctr = 0;
  // while (element.elem != NULL) {
  //   if (key_hash == element.elem->key_hash && strcmp(element.elem->key, key) == 0) {
  //     if (felement.elem == NULL) {
  //       felement.elem = element.elem;
  //     }
  //     elem_ctr++;
  //   }
  //   element.elem = element.elem->next;
  // }
  // if (elem_ctr > 1) {
  //   printf("CTR: %d\n", elem_ctr);
  // }
  // element = felement;

  int traverse_times = 0;
  while (element.elem != NULL && !(key_hash == element.elem->key_hash && strcmp(element.elem->key, key) == 0)) {
    element.elem = element.elem->next;
    traverse_times++;
  }

  return element.elem == NULL ? NULL : element.elem->value;
}

void ht_free(struct ht_table *table) {
  return;
}

struct ht_free_list *ht_clean_prepare(struct ht_table *table) {
  return NULL;
}

int ht_clean_run(struct ht_free_list *free_list) {
  return 0;
}
