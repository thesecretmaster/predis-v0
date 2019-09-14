#ifndef LIB_HASHTABLE_H
#define LIB_HASHTABLE_H

#include <stdbool.h>

struct ht_elem {
  char *key;
  int value;
  struct ht_elem *next;
};

struct ht_free_list {
  struct ht_elem *elem;
  struct ht_free_list *next;
};

struct ht_table {
  struct ht_free_list *free_list;
  struct ht_bucket *root_bucket;
  int bitlen;
};

struct ht_table *ht_init(int);
int ht_find(struct ht_table*, char*);
int ht_store(struct ht_table*, char*, int);
int ht_delete(struct ht_table*, char*);
void ht_free(struct ht_table*);

struct ht_free_list *ht_clean_prepare(struct ht_table*);
int ht_clean_run(struct ht_free_list*);

#endif // LIB_HASHTABLE_H
