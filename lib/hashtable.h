#ifndef LIB_HASHTABLE_H
#define LIB_HASHTABLE_H

#include <stdbool.h>

struct ht_elem {
  char *key;
  int value;
  int skip_status; // 0 = don't skip, 1 = skip, 2 = skip & deletable
  struct ht_elem *next;
};

struct ht_free_list {
  struct ht_elem *elem;
  struct ht_free_list *next;
};

struct ht_table {
  struct ht_free_list *free_list;
  struct ht_elem **elements;
  int size;
  bool write_locked;
  int reader_count;
};

struct ht_table *ht_init(int);
int ht_find(struct ht_table*, char*);
int ht_store(struct ht_table*, char*, int);
int ht_delete(struct ht_table*, char*);

#ifdef HASHTABLE_SAFE
int ht_clean(struct ht_table*);
#else
struct ht_free_list *ht_clean_prepare(struct ht_table*);
int ht_clean_run(struct ht_free_list*);
#endif

#endif // LIB_HASHTABLE_H
