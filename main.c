#include "lib.c"

struct sharedstruct {
  struct thread_info_list *tilist;
  struct thread_info_list *ti_ele;
  struct element_queue **delq;
  struct element_queue **free_list;
  struct main_struct *ms;
  volatile short int weird_lock;
};

#define THREAD_COUNT 5
#define TEST_COUNT 11
#define TEST_RN_COUNT 200
volatile bool start_threads = false;


void *dostuff(void*);
// Each thread has an op_counter which incriments every time an operation is completed
// Del will use shared memory to read each threads op counter, and will wait until each thread
// has incrimented it's op counter.
int main() {
  // GLOBAL SETUP
  data_types[0] = data_type_int;
  struct thread_info_list *tilist = NULL;
  struct element_queue *delq = NULL;
  struct element_queue *free_list = NULL;
  struct main_struct *ms = init();

  struct sharedstruct sharedstruct = (struct sharedstruct){
    .delq = &delq,
    .free_list = &free_list,
    .ms = ms
  };
  pthread_t tid[THREAD_COUNT];
  struct thread_info_list *ti_ele;
  struct thread_info_list *ti_ptr;
  for (int i = 0; i < THREAD_COUNT; i++) {
    ti_ele = malloc(sizeof(struct thread_info_list));
    ti_ele->safe = true;
    ti_ele->next = NULL;

    if (tilist == NULL) {
      tilist = ti_ele;
    } else {
      ti_ptr = tilist;
      while (ti_ptr->next != NULL) { ti_ptr = ti_ptr->next; }
      ti_ptr->next = ti_ele;
    }
    sharedstruct.tilist = tilist;
    sharedstruct.ti_ele = ti_ele;
    sharedstruct.weird_lock = 1;
    pthread_create(&tid[i], NULL, &dostuff, &sharedstruct);
    while (sharedstruct.weird_lock != 0) {}
  }

  printf("Getting this party started\n");
  start_threads = true;

  for (int i = 0; i < THREAD_COUNT; i++) {
    pthread_join(tid[i], NULL);
  }

  printf("Cleanup!\n");
  int tcount = 0;
  struct thread_info_list *tiptr = tilist;
  while (1) {
    if (tiptr == NULL) {break;}
    printf("Thread %d: %s\n", tcount, tiptr->safe ? "true" : "false");
    tcount++;
    tiptr = tiptr->next;
  }
  printf("Used %d threads\n", tcount);

  return 0;
}


void *dostuff(void *ptr) {
  struct sharedstruct *sharedstruct = ptr;
  struct element_queue **delq = sharedstruct->delq;
  struct element_queue **free_list = sharedstruct->free_list;
  struct main_struct *ms = sharedstruct->ms;
  struct thread_info_list *tilist = sharedstruct->tilist;
  struct thread_info_list *ti = sharedstruct->ti_ele;
  safe = &(ti->safe);
  sharedstruct->weird_lock = 0;

  int rns[TEST_RN_COUNT];
  int idx[TEST_RN_COUNT];
  char str[20];
  int errcount = 0;
  int goodcount = 0;
  int geterrors;

  while (start_threads == false) {}
  // int ti_ctr;
  // struct thread_info_list *ti_ptr = NULL;
  struct return_val* rval = malloc(sizeof(struct return_val));
  for (int i = 0; i < TEST_COUNT; i++) {
    for (int j = 0; j < TEST_RN_COUNT; j++) {
      rns[j] = rand();
      snprintf(str, 20, "%d", rns[j]);
      idx[j] = set("int", ms, free_list, str);
      if (idx[j] < 0) {
        printf("Set: %d\n", idx[j]);
      }
    }
    for (int j = 0; j < TEST_RN_COUNT; j++) {
      if (idx[j] < 0) { continue; }
      snprintf(str, 20, "%d", rns[j]);
      geterrors = get("int", ms, rval, idx[j]);
      del(ms, delq, idx[j]);
      if (geterrors != 0) {
        printf("Get: %d\n", geterrors);
      }
      if (strcmp(rval->value, str) != 0) {
        printf("%s != %s\n", rval->value, str);
        errcount++;
      } else {
        goodcount++;
      }
    }
    clean_queue(delq, tilist, free_list);
    // ti_ctr = 0;
    // ti_ptr = tilist;
    // while (ti_ptr != NULL) {
    //   printf("Thread %d: %lld\n", ti_ctr, ti_ptr->op_counter);
    //   ti_ctr++;
    //   ti_ptr = ti_ptr->next;
    // }
  }

  printf("%d errors\n%d good\n", errcount, goodcount);

  // int a = set("int", ms, "123");
  // int b = set("int", ms, "543");
  // int c = set("int", ms, "13333");
  // struct return_val* rval = malloc(sizeof(struct return_val));
  // rval->value = NULL;
  // get("int", ms, rval, a);
  // int ra = strtol(rval->value, NULL, 10);
  // get("int", ms, rval, b);
  // int rb = strtol(rval->value, NULL, 10);
  // get("int", ms, rval, c);
  // int rc = strtol(rval->value, NULL, 10);
  // printf("a: %d\nb: %d\nc: %d\n", ra, rb, rc);
  return NULL;
}
