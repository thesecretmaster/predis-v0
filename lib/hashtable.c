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
  // printf("Init bucket\n");
  return newbucket;
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
    curr_bucket = tmpidx.bucket;
    level++;
    idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
    tmpidx = curr_bucket->elements[idx];
  }
  // Concept:
  // We could stop here and `return &tmpidx`
  // But we also want to build a new bucket and CAS it in
  // if createbucket && tmpidx.elem != NULL
  unsigned int hsh2;
  unsigned int idx2;
  int oidx = idx;
  bool firstitr = true;
  union ht_bucket_elem new_bucket;
  union ht_bucket_elem old_elem = curr_bucket->elements[idx];;
  level++;
  // Note that since we simply overwrite the old elem pointer, we will have some memory leakage.
  if (createbucket && old_elem.elem != NULL && (level * table->bitlen) < sizeof(union ht_bucket_elem)) {
    new_bucket = ht_set_bucket(ht_bucket_init(table));
    idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
    do {
      old_elem = curr_bucket->elements[idx]; // Our guess as to what the elem will be
      while (old_elem.elem != NULL && ht_is_bucket(old_elem)) {
        curr_bucket = old_elem.bucket;
        level++;
        idx = (hsh >> (level * table->bitlen)) & idx_bitmask;
        old_elem = curr_bucket->elements[idx];
      }
      if (old_elem.elem == NULL) {
        // Same as the exit condition of the function
        return &(curr_bucket->elements[idx]);
      }
      if (!firstitr) {
        new_bucket.bucket->elements[idx2].elem = NULL;
      } else {
        firstitr = false;
      }
      hsh2 = old_elem.elem->key_hash;
      idx2 = (hsh2 >> ((level + 1) * table->bitlen)) & idx_bitmask;
      if (!(hsh2 == hsh && strcmp(old_elem.elem->key, key) == 0)) {
        new_bucket.bucket->elements[idx2].elem = old_elem.elem;
      }
    } while (!__atomic_compare_exchange_n(&(curr_bucket->elements[oidx].elem), &old_elem.elem, new_bucket.elem, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    return &(new_bucket.bucket->elements[idx]);
  }
  return &(curr_bucket->elements[idx]);
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
    beptr = ht_elem_internal(table, key, true);
    be = *beptr;
    while (ht_is_bucket(be)) { beptr = ht_elem_internal(table, key, false); be = *beptr; }
    // Assuming elem hasn't been deleted
    if (be.elem == NULL || be.elem->key_hash == key_hash || true) {
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
      new_elem.elem->key_hash = ht_hash(key);
      new_elem.elem->value = value;
      new_elem.elem->next = be.elem;
    } else /* if elem->key_hash != key_hash */ {
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
