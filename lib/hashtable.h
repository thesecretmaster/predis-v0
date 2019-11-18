#ifndef LIB_HASHTABLE_H
#define LIB_HASHTABLE_H

// This is objectively broken. The probem is that any file that includes the
// hashtable will include this header file, and will have its own DEFINE for
// HT_VAL_TYPE. But, when they link with hashtable.c, it will not have that
// DEFINE anymore and so will take this default declaration.
#ifndef HT_VAL_TYPE
#warning "NOT DEFINED"
#define HT_VAL_TYPE void
#endif

struct ht_table {
  volatile struct ht_free_list *free_list;
  struct ht_bucket *root_bucket;
  int bitlen;
  int (*initialize_element)(HT_VAL_TYPE *);
};

struct ht_table *ht_init(int, int (*init_ele)(HT_VAL_TYPE *));
HT_VAL_TYPE* ht_find(struct ht_table*, const char*);
int ht_store(struct ht_table*, const char*, HT_VAL_TYPE*);
int ht_delete(struct ht_table*, char*);
void ht_free(struct ht_table*);
void ht_print(struct ht_table*);
const unsigned int ht_hash(const char*);

struct ht_free_list *ht_clean_prepare(struct ht_table*);
int ht_clean_run(struct ht_free_list*);

#endif // LIB_HASHTABLE_H
