#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

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
  free(buk->elements);
  free(buk);
}

struct ht_table *ht_init(int bitlen) {
  bitlen = 8;
  struct ht_table *table = malloc(sizeof(struct ht_table));
  table->bitlen = bitlen;
  table->allocation_idx = 0;
  table->allocation_incr = 65536; // 2^16
  table->allocation = malloc(sizeof(struct ht_elem)*table->allocation_incr);
  table->free_list = NULL;
  table->root_bucket = ht_bucket_init(table);
  return table;
}

void ht_print(struct ht_table *table) {

}

// Note: Before we return EVER mutate or return the value of this function,
// we need to confirm that it's still an `elem` and not a `bucket`
static union ht_bucket_elem *ht_elem_internal(struct ht_table *table, const char *key, bool createbucket) {
  const unsigned int hsh = ht_hash(key);
  unsigned int level = 0;
  const unsigned int idx_bitmask = ~(~0x0 << table->bitlen);
  unsigned int idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
  struct ht_bucket *curr_bucket = table->root_bucket;
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
  // What if we need to alloc multiple levels?
  // I.e. idx2 == nidx
  do {
    ht_get_bucket(new_bucket)->elements[idx2].elem = NULL;
    tmpidx = curr_bucket->elements[idx];
    while (ht_is_bucket(tmpidx)) {
      curr_bucket = ht_get_bucket(tmpidx);
      level++;
      idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
      tmpidx = curr_bucket->elements[idx];
    }
    nidx = (hsh >> (level * table->bitlen)) & idx_bitmask;
    idx2 = (tmpidx.elem->key_hash >> (level * table->bitlen)) & idx_bitmask;
    ht_get_bucket(new_bucket)->elements[idx2] = tmpidx;
  } while (!__atomic_compare_exchange_n(&(curr_bucket->elements[idx].elem), &tmpidx.elem, new_bucket.elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  if (nidx == idx2) {
    return ht_elem_internal(table, key, createbucket);
  }
  return &(ht_get_bucket(new_bucket)->elements[nidx]);
}

HT_VAL_TYPE* ht_find(struct ht_table *table, const char *key) {
  union ht_bucket_elem be = *ht_elem_internal(table, key, false);
  while (ht_is_bucket(be)) { be = *ht_elem_internal(table, key, false); }
  struct ht_elem *elem = be.elem;
  if (elem == NULL) {
    return NULL;
  } else {
    unsigned int key_hash = ht_hash(key);
    // This is just while (key != elem->key) but with speed improvements
    while (!(key_hash == elem->key_hash && strcmp(key, elem->key) == 0)) {
      elem = elem->next;
      if (elem == NULL) {
        return NULL;
      }
    }
    return elem->value;
  }
}
// Idea: Different hashes for the linked lists and the root elements
// Idea: Use const for the key and pass the hash into ht_elem_internal
int ht_store(struct ht_table *table, const char *key, HT_VAL_TYPE *value) {
  const unsigned int key_hash = ht_hash(key);
  union ht_bucket_elem *beptr;
  union ht_bucket_elem be;
  int idx;
  union ht_bucket_elem new_elem;
  do {
    do {
      beptr = ht_elem_internal(table, key, true);
      be = *beptr;
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
      return -1; // This would make literally no sense based on how the HT is structured
    }
    // In my perfect world, c would let me use just * instead of *.elem, but it
    // is too dumb to know that a union of pointers is just a pointer.
  } while (!__atomic_compare_exchange_n((struct ht_elem**)beptr, &be.elem, new_elem.elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
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
