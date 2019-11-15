#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
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
bool ht_is_bucket(union ht_bucket_elem e) { return (uintptr_t)e.bucket & 0x1; }
union ht_bucket_elem ht_set_bucket(struct ht_bucket* bucket) { return (union ht_bucket_elem)(struct ht_bucket*)((uintptr_t)bucket | 0x1); }
union ht_bucket_elem ht_set_elem(struct ht_elem* elem) { return (union ht_bucket_elem)(struct ht_elem*)((uintptr_t)elem & (~0x1)); }
struct ht_bucket *ht_get_bucket(union ht_bucket_elem be) { assert(ht_is_bucket(be)); return (struct ht_bucket*)((uintptr_t)be.bucket & (~0x1)); }

// https://stackoverflow.com/a/7666577/4948732
const unsigned int ht_hash(const char *str) {
  int hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

// unsigned int ht_hash(char *key)
// {
//     int len = strlen(key);
//     uint32_t hash, i;
//     for(hash = i = 0; i < len; ++i)
//     {
//         hash += key[i];
//         hash += (hash << 10);
//         hash ^= (hash >> 6);
//     }
//     hash += (hash << 3);
//     hash ^= (hash >> 11);
//     hash += (hash << 15);
//     return hash;
// }

static struct ht_bucket *ht_bucket_init(struct ht_table *table) {
  struct ht_bucket *newbucket = malloc(sizeof(struct ht_bucket));
  int element_len = 0x1 << table->bitlen;
  newbucket->elements = malloc(sizeof(union ht_bucket_elem) * element_len);
  for (int i = 0; i < element_len; i++) {
    newbucket->elements[i].elem = NULL;
  }
  return newbucket;
}

static void ht_free_bucket(struct ht_bucket *buk) {
  free((union ht_bucket_elem*)buk->elements);
  free(buk);
}

struct ht_table *ht_init(int bitlen) {
  bitlen = 8;
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->bitlen = bitlen;
  table->allocation_idx = 0;
  // table->tlock = 0;
  table->allocation_incr = 65536; // 2^16
  table->allocation = malloc(sizeof(struct ht_elem)*table->allocation_incr);
  table->free_list = NULL;
  table->root_bucket = ht_bucket_init(table);
  return table;
}

void ht_print(struct ht_table *table) {

}
#include <stdio.h>
// Note: Before we return EVER mutate or return the value of this function,
// we need to confirm that it's still an `elem` and not a `bucket`
static volatile union ht_bucket_elem *ht_elem_internal(struct ht_table *table, const char *key, bool createbucket) {
  const unsigned int hsh = ht_hash(key);
  unsigned int level = 0;
  const unsigned int idx_bitmask = ~(~0x0 << table->bitlen);
  unsigned int idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
  volatile struct ht_bucket *curr_bucket = table->root_bucket;
  union ht_bucket_elem tmpidx = curr_bucket->elements[idx];
  while (tmpidx.elem != NULL && ht_is_bucket(tmpidx)) {
    curr_bucket = ht_get_bucket(tmpidx);
    level++;
    idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
    tmpidx = curr_bucket->elements[idx];
  }
  if (tmpidx.elem == NULL || !createbucket || tmpidx.elem->key_hash == hsh) {
    return &(curr_bucket->elements[idx]);
  }
  unsigned int nidx, idx2;
  union ht_bucket_elem new_bucket = ht_set_bucket(ht_bucket_init(table));
  level++;
  idx2 = 0;
  bool firstitr = true;
  int itrs =0;
  do {
    if (!firstitr) {
      ht_get_bucket(new_bucket)->elements[idx2].elem = NULL;
      // NOTE: We're not actually doing anything with .elem, unions of pointers just suck.
      tmpidx.elem = __atomic_load_n(&(curr_bucket->elements[idx].elem), __ATOMIC_SEQ_CST);
      while (tmpidx.elem != NULL && ht_is_bucket(tmpidx)) {
        curr_bucket = ht_get_bucket(tmpidx);
        level++;
        idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
        // NOTE: We're not actually doing anything with .elem, unions of pointers just suck.
        tmpidx.elem = __atomic_load_n(&(curr_bucket->elements[idx].elem), __ATOMIC_SEQ_CST);
      }
      if (tmpidx.elem == NULL || tmpidx.elem->key_hash == hsh) {
        return &(curr_bucket->elements[idx]);
      }
    } else {
      firstitr = false;
    }
    itrs++;
    nidx = (hsh >> (level * table->bitlen)) & idx_bitmask;
    idx2 = (tmpidx.elem->key_hash >> (level * table->bitlen)) & idx_bitmask;
    ht_get_bucket(new_bucket)->elements[idx2] = tmpidx;
  } while (!__atomic_compare_exchange_n(&(curr_bucket->elements[idx].elem), &tmpidx.elem, new_bucket.elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  if (itrs > 1) {
    printf("Competed %d\n", itrs);
  }
  if (nidx == idx2) {
    return ht_elem_internal(table, key, createbucket);
  }
  return &(ht_get_bucket(new_bucket)->elements[nidx]);
}

HT_VAL_TYPE* ht_find(struct ht_table *table, const char *key) {
  // while (!__atomic_test_and_set(&(table->tlock), __ATOMIC_SEQ_CST)) {};
  union ht_bucket_elem be;
  do {
    be = *ht_elem_internal(table, key, false);
  } while (ht_is_bucket(be));
  struct ht_elem *elem = be.elem;
  if (elem == NULL) {
    // __atomic_clear(&(table->tlock), __ATOMIC_SEQ_CST);
    return NULL;
  } else {
    unsigned int key_hash = ht_hash(key);
    int itrs = 0;
    // This is just while (key != elem->key) but with speed improvements
    while (!(key_hash == elem->key_hash && strcmp(key, elem->key) == 0)) {
      elem = elem->next;
      itrs++;
      if (elem == NULL) {
        // __atomic_clear(&(table->tlock), __ATOMIC_SEQ_CST);
        return NULL;
      }
    }
    // __atomic_clear(&(table->tlock), __ATOMIC_SEQ_CST);
    return elem->value;
  }
}
// Idea: Different hashes for the linked lists and the root elements
// Idea: Use const for the key and pass the hash into ht_elem_internal
int ht_store(struct ht_table *table, const char *key, HT_VAL_TYPE *value) {
  // while (!__atomic_test_and_set(&(table->tlock), __ATOMIC_SEQ_CST)) {};
  const unsigned int key_hash = ht_hash(key);
  volatile union ht_bucket_elem *beptr;
  union ht_bucket_elem be;
  int idx;
  union ht_bucket_elem new_elem;
  do {
    do {
      beptr = ht_elem_internal(table, key, true);
      // NOTE: We're not actually doing anything with (struct ht_elem), unions of pointers just suck.
      be.elem = __atomic_load_n((struct ht_elem**)beptr, __ATOMIC_SEQ_CST);
    } while (ht_is_bucket(be));
    // Assuming elem hasn't been deleted
    if (be.elem == NULL || be.elem->key_hash == key_hash) {
      idx = __atomic_fetch_add(&(table->allocation_idx), 1, __ATOMIC_SEQ_CST);
      if (idx == table->allocation_incr) {
        __atomic_store_n(&(table->allocation), malloc(sizeof(struct ht_elem)*table->allocation_incr), __ATOMIC_SEQ_CST);
        idx = 0;
        __atomic_store_n(&(table->allocation_idx), 1, __ATOMIC_SEQ_CST);
      } else if (idx > table->allocation_incr) {
        while (idx > table->allocation_idx) {
          idx = __atomic_fetch_add(&(table->allocation_idx), 1, __ATOMIC_SEQ_CST);
        }
      }
      new_elem.elem = table->allocation + (idx % table->allocation_incr);
      new_elem.elem->key = strdup(key);
      new_elem.elem->key_hash = key_hash;
      new_elem.elem->value = value;
      new_elem.elem->next = be.elem;
    } else /* if elem->key_hash != key_hash */ {
      // printf("%08X\n%08X\n", be.elem->key_hash, key_hash);
      // __atomic_clear(&(table->tlock), __ATOMIC_SEQ_CST);
      return -1; // This would make literally no sense based on how the HT is structured
    }
    // In my perfect world, c would let me use just * instead of *.elem, but it
    // is too dumb to know that a union of pointers is just a pointer.
  } while (!__atomic_compare_exchange_n((struct ht_elem**)beptr, &be.elem, new_elem.elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  // __atomic_clear(&(table->tlock), __ATOMIC_SEQ_CST);
  return 0;
}

int ht_delete(struct ht_table *table, char *key) {
  struct ht_elem **elem = &((*(ht_elem_internal(table, key, false))).elem);
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
