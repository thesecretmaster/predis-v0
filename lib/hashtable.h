#ifndef LIB_HASHTABLE_H
#define LIB_HASHTABLE_H

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
};

struct ht_table *ht_init(int);
int ht_find(struct ht_table*, char*);
int ht_store(struct ht_table*, char*, int);
int ht_delete(struct ht_table*, char*);

#endif // LIB_HASHTABLE_H