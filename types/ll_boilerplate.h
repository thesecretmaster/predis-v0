#ifdef GEN_DT_LL

static struct data_type_ll DT_LL_FULL_NAME = {
#ifndef PREV_DT_LL
  .prev = NULL,
#else
  .prev = &PREV_DT_LL,
#endif
  .type = &DT_FULL_NAME
};

#endif // GEN_DT_LL
