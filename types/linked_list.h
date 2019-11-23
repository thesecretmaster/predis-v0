#include "type_ll.h"

#undef DT_FULL_NAME
#undef DT_LL_FULL_NAME
#define DT_FULL_NAME data_type_linked_list
#define DT_LL_FULL_NAME dt_ll_linked_list

extern const struct data_type DT_FULL_NAME;

#include "ll_boilerplate.h"

#ifdef GEN_DT_LL

#undef PREV_DT_LL
#define PREV_DT_LL dt_ll_linked_list

#endif
