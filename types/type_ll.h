#ifndef TYPE_LL_H
#define TYPE_LL_H

#include "../data_type.h"

struct data_type_ll {
  const struct data_type *type;
  struct data_type_ll *prev;
};

#endif
