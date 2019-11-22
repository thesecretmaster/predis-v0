#ifndef PREDIS_H
#define PREDIS_H

#include "data_type.h"
#include <stdbool.h>
#include <stddef.h>
#include "hashtable-shared/struct main_ele.h"
#include "lib/hashtable.struct main_ele.h"

extern volatile __thread bool *safe;

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
  struct element_queue *deletion_queue;
  struct element_queue *free_list;
  struct thread_info_list *thread_list;
  struct ht_table *hashtable;
  volatile struct main_ele *allocation;
  int allocation_incr;
  volatile int allocation_idx;
  volatile int thread_list_traversing_count;
  volatile bool thread_list_write_locked;
};

struct main_struct* init(int);
int set(char*, struct main_struct*, char*, char*, char**);
int get(char*, struct main_struct*, char*, struct return_val*, char*, char**);
int update(char*, char*, struct main_struct*, char*, char**);
int del(struct main_struct*, char*);
int clean_queue(struct main_struct*);
int free_predis(struct main_struct*);
struct thread_info_list* register_thread(struct main_struct*);
void deregister_thread(struct main_struct*, struct thread_info_list*);

#endif // PREDIS_H
