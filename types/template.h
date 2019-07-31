#ifndef TYPE_H
#define TYPE_H

#include "../data_type.h"

static int get(void*, struct return_val*);
static int set(void**, char*);
static void *clone(void*);
static int free_ele(void*);

#endif // TYPE_H
