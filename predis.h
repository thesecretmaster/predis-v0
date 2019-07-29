#ifndef PREDIS_H
#define PREDIS_H

#include "data_type.h"
#include <stdbool.h>
#include <stddef.h>

extern volatile __thread bool *safe;

struct main_ele {
  char* type;
  void* ptr;
  bool pending_delete;
};

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
};

int clean_queue(struct main_struct*);
struct main_struct* init(int);
int set(char*, struct main_struct*, char*);
int get(char*, struct main_struct*, struct return_val*, int);
int update(char*, struct main_struct*, char*, int);
int del(struct main_struct*, int);
int clean_queue(struct main_struct*);

#endif // PREDIS_H
