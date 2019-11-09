#ifndef PREDIS_H
#define PREDIS_H

#include "data_type.h"
#include <stdbool.h>
#include <stddef.h>

extern volatile __thread bool *safe;

struct main_ele {
  const struct data_type* type;
  void* ptr;
  bool pending_delete;
};

// #define str(str) #str
// #define str2(str1) str(str1)
#define HT_VAL_TYPE struct main_ele
// #pragma message str2(HT_VAL_TYPE)
#include "lib/hashtable.h"

struct thread_info_list {
  struct thread_info_list *next;
  volatile bool safe;
};

struct element_queue {
  struct element_queue *next;
  struct main_ele *element;
  int idx;
};

struct main_struct {
  int size;
  int counter;
  struct main_ele *elements;
  struct element_queue *deletion_queue;
  struct element_queue *free_list;
  struct thread_info_list *thread_list;
  struct ht_table *hashtable;
  struct main_ele *allocation;
  int allocation_incr;
  int allocation_idx;
  int thread_list_traversing_count;
  bool thread_list_write_locked;
};

struct main_struct* init(int);
int set(char*, struct main_struct*, char*, char*);
int get(char*, struct main_struct*, struct return_val*, char*);
int update(char*, char*, struct main_struct*, char*, char*);
int del(struct main_struct*, char*);
int clean_queue(struct main_struct*);
int free_predis(struct main_struct*);
struct thread_info_list* register_thread(struct main_struct*);
void deregister_thread(struct main_struct*, struct thread_info_list*);

#endif // PREDIS_H
