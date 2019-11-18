#ifndef PREDIS_ELE_H
#define PREDIS_ELE_H

#include <stdbool.h>

struct main_ele {
  const struct data_type* type;
  void* ptr;
  bool pending_delete;
};

#endif
