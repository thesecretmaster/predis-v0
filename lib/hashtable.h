#ifndef LIB_HASHTABLE_H
#define LIB_HASHTABLE_H

#include <stdbool.h>

#ifndef HT_VAL_TYPE
#define HT_VAL_TYPE int
#endif

struct ht_elem {
  char *key;
  unsigned int key_hash;
  HT_VAL_TYPE* value;
  struct ht_elem *next;
};

struct ht_free_list {
  struct ht_elem *elem;
  struct ht_free_list *next;
};

struct ht_table {
  volatile int allocation_idx;
  int allocation_incr;
  volatile int tlock;
  struct ht_elem *allocation;
  volatile struct ht_free_list *free_list;
  struct ht_bucket *root_bucket;
  int bitlen;
};

struct ht_table *ht_init(int);
HT_VAL_TYPE* ht_find(struct ht_table*, const char*);
int ht_store(struct ht_table*, const char*, HT_VAL_TYPE*);
int ht_delete(struct ht_table*, char*);
void ht_free(struct ht_table*);
void ht_print(struct ht_table*);
const unsigned int ht_hash(const char*);

struct ht_free_list *ht_clean_prepare(struct ht_table*);
int ht_clean_run(struct ht_free_list*);

#endif // LIB_HASHTABLE_H
