#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sched.h>
#include <stdalign.h>
#include <string.h>
#include "linked_list.h"

struct linked_list {
  struct linked_list_elem *head;
  struct linked_list_elem *tail;
  // struct deleted_linked_list_elem * volatile delete_list_head;
  // struct deleted_linked_list_elem * volatile delete_list_tail;
  // volatile bool delete_list_lock;
  // volatile unsigned int delete_list_ctr;
  volatile unsigned int length;
  // volatile unsigned int user_count;
  // volatile bool clean_lock;
};

struct linked_list_elem {
  struct linked_list_elem * volatile next;
  struct linked_list_elem * volatile prev;
  volatile int refcount;
  volatile int reader_lock;
  volatile int writer_lock;
  volatile bool deleted;
  struct llval value;
};

/* inline */ int get_length(struct linked_list *ll) {
  return ll->length;
}

/* inline */ struct llval *get_value(struct linked_list_elem *elem) {
  return &elem->value;
}

/* inline */ struct linked_list_elem* get_tail(struct linked_list *ll) {
  return ll->tail;
}

/* inline */ struct linked_list_elem* get_head(struct linked_list *ll) {
  return ll->head;
}

static void incr_reader_lock(struct linked_list_elem *elem) {
  while (true) {
    while (__atomic_load_n(&elem->writer_lock, __ATOMIC_SEQ_CST) != 0) {}
    __atomic_add_fetch(&elem->reader_lock, 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&elem->writer_lock, __ATOMIC_SEQ_CST) != 0) {
      __atomic_add_fetch(&elem->reader_lock, -1, __ATOMIC_SEQ_CST);
      continue;
    } else {
      break;
    }
  }
  return;
}

static void decr_reader_lock(struct linked_list_elem *elem) {
  __atomic_add_fetch(&elem->reader_lock, -1, __ATOMIC_SEQ_CST);
  return;
}

/* __inline__ */ struct linked_list_elem* get_next(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease) {
  if (get_tail(ll) == elem) {
    return NULL;
  }
  retry:
  incr_reader_lock(elem);
  if (elem->deleted) {
    decr_reader_lock(elem);
    elem = elem->prev;
    goto retry;
  }
  decr_reader_lock(elem);
  return elem->next;
}

/* __inline__ */ struct linked_list_elem* get_prev(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease) {
  if (get_head(ll) == elem) {
    return NULL;
  }
  retry:
  incr_reader_lock(elem);
  if (elem->deleted) {
    decr_reader_lock(elem);
    elem = elem->next;
    goto retry;
  }
  decr_reader_lock(elem);
  return elem->prev;
}
/* __inline__ */ void release(struct linked_list *ll, struct linked_list_elem *elem) {

}
/* __inline__ */ struct linked_list_elem* clone_ref(struct linked_list *ll, struct linked_list_elem *elem) {
return elem;
}

static bool try_get_writer_lock(struct linked_list_elem *elem) {
  int zero = 0;
  int one = 1;
  if (__atomic_compare_exchange(&elem->writer_lock, &zero, &one, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    while (__atomic_load_n(&elem->reader_lock, __ATOMIC_SEQ_CST) != 0) {}
    return true;
  } else {
    return false;
  }
}

static void get_writer_lock(struct linked_list_elem *elem) {
  int zero;
  int one;
  do {
    zero = 0;
    one = 1;
  } while (!__atomic_compare_exchange(&elem->writer_lock, &zero, &one, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  while (__atomic_load_n(&elem->reader_lock, __ATOMIC_SEQ_CST) != 0) {}
  return;
}

static void release_writer_lock(struct linked_list_elem *elem) {
  __atomic_store_n(&elem->writer_lock, 0, __ATOMIC_SEQ_CST);
  return;
}

int insert_after(struct linked_list *ll, struct linked_list_elem *prev, struct llval value) {
  // The tail is immutable, and it's illegal to append to the tail
  if (ll->tail == prev) {
    return -10;
  }

  // Allocate new element
  struct linked_list_elem *ele = malloc(sizeof(struct linked_list_elem));
  // Metadata
  ele->refcount = 0;
  ele->reader_lock = 0;
  ele->writer_lock = 0;
  // Value (could be rewritten using memcpy, I think)
  ele->value.thread_num = value.thread_num;
  ele->value.ctr = value.ctr;
  ele->value.deleteme = value.deleteme;

  retry:
  // Get the writer lock on prev
  get_writer_lock(prev);

  if (prev->deleted) {
    release_writer_lock(prev);
    return -1;
  }

  struct linked_list_elem *next = prev->next;
  if (!try_get_writer_lock(next)) {
    release_writer_lock(prev);
    goto retry;
  }

  if (next->deleted) {
    release_writer_lock(prev);
    release_writer_lock(next);
    return -1;
  }

  // At this point, we have a writer lock on prev and know it is and will not
  // be deleted. We also know that prev->next will not change and will not
  // be deleted because deletion requires a lock on both prev and next.
  ele->prev = prev;
  ele->next = next;
  next->prev = ele;
  prev->next = ele;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  release_writer_lock(prev);
  release_writer_lock(next);
  return 0;
}

int delete_elem(struct linked_list *ll, struct linked_list_elem *elem) {
  if (elem == get_head(ll) || elem == get_tail(ll)) {
    return -1;
  }
  retry:

  get_writer_lock(elem);
  if (elem->deleted) {
    release_writer_lock(elem);
    return -2;
  }

  struct linked_list_elem *prev;
  // do {
    prev = elem->prev;
    if (!try_get_writer_lock(prev)) {
      release_writer_lock(elem);
      goto retry;
    }
    if (prev->deleted) {
      release_writer_lock(elem);
      release_writer_lock(prev);
      return -3;
    }
  // } while (true);

  struct linked_list_elem *next;
  // do {
    next = elem->next;
    if (!try_get_writer_lock(next)) {
      release_writer_lock(elem);
      release_writer_lock(prev);
      goto retry;
    }
    if (next->deleted) {
      release_writer_lock(elem);
      release_writer_lock(prev);
      release_writer_lock(next);
      return -4;
    }
  // } while (true);

  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;
  elem->deleted = true;
  release_writer_lock(elem);
  release_writer_lock(prev);
  release_writer_lock(next);
  return 0;
}

int clean(struct linked_list *ll);

struct linked_list *initialize_ll(void) {
  if (alignof(struct linked_list_elem) <= ~((unsigned int)(~0x0) << 2)) {
    printf("Yikes, we can't store the tags we need (%lu)! Erroring...\n", alignof(struct linked_list_elem));
    return NULL;
  }
  struct linked_list *ll = malloc(sizeof(struct linked_list));
  ll->length = 0;
  ll->head = malloc(sizeof(struct linked_list_elem));
  ll->tail = malloc(sizeof(struct linked_list_elem));
  ll->head->next = ll->tail;
  ll->tail->prev = ll->head;
  ll->head->prev = NULL;
  ll->tail->next = NULL;
  ll->head->refcount = 0;
  ll->tail->refcount = 0;
  ll->head->value.thread_num = -7;
  ll->tail->value.thread_num = -8;
  ll->head->value.deleteme = false;
  ll->tail->value.deleteme = false;
  ll->head->value.ctr = -6;
  ll->tail->value.ctr = -9;
  ll->head->reader_lock = 0;
  ll->head->writer_lock = 0;
  ll->tail->reader_lock = 0;
  ll->tail->writer_lock = 0;
  ll->head->deleted = false;
  ll->tail->deleted = false;
  return ll;
}

// I know you hate this, but you need to use reader/writer per-elem locks :'(
// Don't worry, you still get to have fun with how deletion will work. So like we don't
// need to hold locks while we're sitting operating on the value in one element.
