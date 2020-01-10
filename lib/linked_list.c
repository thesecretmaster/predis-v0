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
  struct deleted_linked_list_elem * volatile delete_list_head;
  struct deleted_linked_list_elem * volatile delete_list_tail;
  volatile bool delete_list_lock;
  volatile unsigned int delete_list_ctr;
  volatile unsigned int length;
  volatile unsigned int user_count;
  volatile bool clean_lock;
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

// struct llval {
//   int thread_num;
//   int ctr;
// };

// struct linked_list_elem {
//   struct linked_list_elem * volatile next;
//   struct linked_list_elem * volatile prev;
//   volatile unsigned int refcount;
//   volatile bool delete_lock;
//   struct llval value;
// };

struct deleted_linked_list_elem {
  struct linked_list_elem *elem;
  struct deleted_linked_list_elem *next;
  struct deleted_linked_list_elem *prev;
};

// static inline void safe_atomic_incr(int *value, int qty) {
//   int v, ov;
//   do {
//     ov = __atomic_load_n(value, __ATOMIC_SEQ_CST);
//     v = ov + qty;
//     v = v < 0 ? 0 : v;
//   } while (!__atomic_compare_exchange(value, &ov, &v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
// }

static inline bool marked_as_deleted(struct linked_list_elem *elem) {
  return ((uintptr_t)elem & 0x1) == 0x1;
}

static inline struct linked_list_elem *clear_mark(struct linked_list_elem *elem) {
  return (struct linked_list_elem *)((uintptr_t)elem & ~0x1);
}

static inline struct linked_list_elem* mark_as_deleted(struct linked_list_elem *elem) {
  return (struct linked_list_elem *)((uintptr_t)elem | 0x1);
}

struct linked_list *initialize_ll(void) {
  if (alignof(struct linked_list_elem) <= ~((unsigned int)(~0x0) << 2)) {
    printf("Yikes, we can't store the tags we need (%lu)! Erroring...\n", alignof(struct linked_list_elem));
    return NULL;
  }
  struct linked_list *ll = malloc(sizeof(struct linked_list));
  ll->length = 0;
  ll->user_count = 0;
  ll->delete_list_head = NULL;
  ll->delete_list_tail = NULL;
  ll->delete_list_lock = false;
  ll->delete_list_ctr = 0;
  ll->clean_lock = false;
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
  return ll;
}

// int push_head(struct linked_list *ll, const char *value) {
//   struct linked_list_elem *ele = malloc(sizeof(struct linked_list_elem));
//   ele->value = strdup(value);
//   struct linked_list_elem *head;
//   do {
//     head = ll->head;
//     ele->prev = head;
//     ele->next = head->next;
//     if (marked_as_deleted(ele->next)) {
//       continue;
//     }
//   } while (!__atomic_compare_exchange(&head->next, &ele->next, &ele, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
//
// }

int clean(struct linked_list *ll) {
  // AQUIRE RW LOCK
  bool fls, tru;
  do {
    fls = false;
    tru = true;
  } while (!__atomic_compare_exchange(&ll->delete_list_lock, &fls, &tru, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  while (__atomic_load_n(&ll->delete_list_ctr, __ATOMIC_SEQ_CST) != 0) {}
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  struct deleted_linked_list_elem *tail = ll->delete_list_tail;

  while (tail != NULL) {
    if (tail->elem->refcount == 0) {
      if (tail->prev == NULL) {
        ll->delete_list_head = tail->next;
      } else {
        tail->prev->next = tail->next;
      }
      if (tail->next == NULL) {
        ll->delete_list_tail = tail->prev;
      } else {
        tail->next->prev = tail->prev;
      }
      free(tail->elem);
      free(tail);
    } else {
      while (marked_as_deleted(clear_mark(tail->elem->next)->next) /* next is marked as deleted */) {
        tail->elem->next = clear_mark(tail->elem->next)->next; /* point past next */
      }
      while (marked_as_deleted(clear_mark(tail->elem->prev)->prev) /* prev is marked as deleted */) {
        tail->elem->prev = clear_mark(tail->elem->prev)->prev; /* point past prev */
      }
    }
    tail = tail->prev;
  }

  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  __atomic_store_n(&ll->delete_list_lock, false, __ATOMIC_SEQ_CST);
  return 0;
  // bool not_held, held;
  // do {
  //   not_held = false;
  //   held = true;
  // } while (!__atomic_compare_exchange(&ll->clean_lock, &not_held, &held, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  // while (__atomic_load_n(&ll->user_count, __ATOMIC_SEQ_CST) > 0) {};
  // struct deleted_linked_list_elem *delem;
  // struct deleted_linked_list_elem *dlist_base = ll->delete_list;
  // struct linked_list_elem *ptr;
  // // First, make sure all the live pointers are actually live
  // delem = dlist_base;
  // while (delem != NULL) {
  //   ptr = delem->prev_live;
  //   while (marked_as_deleted(ptr->next)) {
  //     ptr = clear_mark(ptr->prev);
  //   }
  //   delem->prev_live = ptr;
  //
  //   ptr = delem->next_live;
  //   while (marked_as_deleted(ptr->next)) {
  //     ptr = clear_mark(ptr->next);
  //   }
  //   delem->next_live = ptr;
  //
  //   delem = delem->next;
  // }
  //
  // // Now free all the things with refcount == 0
  // struct deleted_linked_list_elem *tofree;
  // struct deleted_linked_list_elem *prev;
  // delem = dlist_base;
  // while (delem != NULL) {
  //   tofree = NULL;
  //   if (delem->elem->refcount == 0) {
  //     tofree = delem;
  //     prev->next = delem->next;
  //   }
  //   prev = delem;
  //   delem = delem->next;
  //   if (tofree != NULL) {
  //     free(tofree->elem);
  //     free(tofree);
  //   }
  // }
  return 0;
}

// static /* inline */ void linked_list_enter(struct linked_list *ll) {
//   while (true) {
//     while (__atomic_load_n(&ll->clean_lock, __ATOMIC_SEQ_CST)) {}
//     __atomic_add_fetch(&ll->user_count, 1, __ATOMIC_SEQ_CST);
//     if (__atomic_load_n(&ll->clean_lock, __ATOMIC_SEQ_CST)) {
//       __atomic_add_fetch(&ll->user_count, -1, __ATOMIC_SEQ_CST);
//     } else {
//       break;
//     }
//   }
// }

int delete_elem(struct linked_list *ll, struct linked_list_elem *elem) {
  if (elem == ll->head || elem == ll->tail || elem == NULL) {
    return -1;
  }
  struct linked_list_elem *prev;
  struct linked_list_elem *next;
  struct linked_list_elem *dprev;
  struct linked_list_elem *dnext;
  // __atomic_store_n(&elem->delete_lock, true, __ATOMIC_SEQ_CST);
  do {
    prev = elem->prev;
    next = elem->next;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (marked_as_deleted(prev) || marked_as_deleted(next)) {
      return -1; // delete already in progress
    }
    dprev = mark_as_deleted(prev);
    dnext = mark_as_deleted(next);
    if (__atomic_compare_exchange(&elem->next, &next, &dnext, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      if (__atomic_compare_exchange(&elem->prev, &prev, &dprev, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        break;
      } else {
        // If the first succeeds and the second fails, we'll revert the first before trying again
        __atomic_compare_exchange(&elem->next, &dnext, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
      }
    }
  } while (true);
  // We can actually delete it!
  // First, walk up prevs next pointer
  struct linked_list_elem *tmp;
  unsigned int itrs = 0;
  struct linked_list_elem *orig_prev = prev;
  struct linked_list_elem *telem = elem;
  while (!__atomic_compare_exchange(&prev->next, &telem, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    telem = elem;
    prev = prev->next;
    itrs++;
    if (marked_as_deleted(prev)) {
      prev = orig_prev;
    }
  }
  telem = elem;
  // Then, try to set nexts prev pointer. We don't care if we fail, because that means somebody else has set it.
  __atomic_compare_exchange(&next->prev, &telem, &prev, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

  // while (__atomic_load_n(&prev->delete_lock, __ATOMIC_SEQ_CST)) {}
  // while (__atomic_load_n(&next->delete_lock, __ATOMIC_SEQ_CST)) {}
  // struct deleted_linked_list_elem *dwrap = malloc(sizeof(struct deleted_linked_list_elem));
  // // dwrap->next_live = clear_mark(elem->next);
  // // dwrap->prev_live = clear_mark(elem->prev);
  // dwrap->prev = NULL;
  // dwrap->elem = elem;
  //
  // // INCR RW LOCK
  // while (true) {
  //   while (__atomic_load_n(&ll->delete_list_lock, __ATOMIC_SEQ_CST)) {}
  //   __atomic_add_fetch(&ll->delete_list_ctr, 1, __ATOMIC_SEQ_CST);
  //   if (__atomic_load_n(&ll->delete_list_lock, __ATOMIC_SEQ_CST)) {
  //     __atomic_add_fetch(&ll->delete_list_ctr, -1, __ATOMIC_SEQ_CST);
  //   } else {
  //     break;
  //   }
  // }
  //
  // do {
  //   dwrap->next = ll->delete_list_head;
  // } while (!__atomic_compare_exchange(&ll->delete_list_head, &dwrap->next, &dwrap, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  // if (dwrap->next == NULL /* && ll->delete_list_tail == NULL */) {
  //   __atomic_store(&ll->delete_list_tail, &dwrap, __ATOMIC_SEQ_CST);
  // } else if (dwrap->next != NULL) {
  //   __atomic_store(&dwrap->next->prev, &dwrap, __ATOMIC_SEQ_CST);
  // }
  // // DINCR RW LOCK
  // __atomic_add_fetch(&ll->delete_list_ctr, -1, __ATOMIC_SEQ_CST);
  // __atomic_store_n(&elem->delete_lock, false, __ATOMIC_SEQ_CST);
  return 0;
}

int insert_after(struct linked_list *ll, struct linked_list_elem *prev, struct llval value) {
  if (marked_as_deleted(prev) || prev == NULL || prev == ll->tail) {
    return -10;
  }
  struct linked_list_elem *ele = malloc(sizeof(struct linked_list_elem));
  ele->refcount = 0;
  ele->delete_lock = false;
  ele->value.thread_num = value.thread_num;
  ele->value.ctr = value.ctr;
  ele->value.deleteme = value.deleteme;
  struct linked_list_elem *prev_next;
  do {
    prev_next = prev->next;
    // Need to confirm to make sure that if prev was deleted, we don't propogate that deletion to the new element.
    if (marked_as_deleted(prev_next)) {
      // Shoot! Prev was deleted from under us!
      // In theory we could backtrack or do some magic to recover, but
      // we can also just fail??
      return -1;
      // if (prev == ll->head || prev == NULL) {
      //   return -1;
      // }
      // continue; // restart loop
    }
    ele->next = prev_next;
    ele->prev = prev;
  } while (!__atomic_compare_exchange(&prev->next, &ele->next, &ele, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  __atomic_add_fetch(&ll->length, 1, __ATOMIC_SEQ_CST);
  struct linked_list_elem *tprev = prev;
  // Now we have it inserted in the forwards direction. Time to swing the prev pointer.
  // while (!__atomic_compare_exchange(&prev_next->prev, &tprev, &ele, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
  //   // If the CAS fails, that's OK in every case EXCEPT if there's an insertion after ele s.t. ele->next != next
  //   tprev = prev;
  //   if (ele->next == prev_next) {
  //     break;
  //   }
  //   // ele = ele->next;
  // }
  // Thinking about this as swinging a pointer is wrong. This is an independant
  // operation which simply fixes prev_next's prev pointer because we've messed
  // it up.
  // do {
  //   prev = prev_next->prev;
  //   tprev = prev;
  //   if (marked_as_deleted(tprev) || tprev == ele) {
  //     break;
  //   }
  //   while (tprev->next != prev_next) {
  //     tprev = tprev->next;
  //     if (marked_as_deleted(tprev) || tprev == ele || marked_as_deleted(prev_next->next)) {
  //       break;
  //     }
  //   }
  // } while (!__atomic_compare_exchange(&prev_next->prev, &prev, &tprev, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
  __atomic_compare_exchange(&prev_next->prev, &prev, &ele, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  return 0;
}

/* inline */ void release(struct linked_list *ll, struct linked_list_elem *elem) {
  if (elem == NULL)
    return;
  __atomic_add_fetch(&elem->refcount, -1, __ATOMIC_SEQ_CST);
}

struct linked_list_elem *clone_ref(struct linked_list *ll, struct linked_list_elem *elem) {
  __atomic_add_fetch(&elem->refcount, 1, __ATOMIC_SEQ_CST);
  return elem;
}

/* inline */ struct linked_list_elem* get_next(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease) {
  struct linked_list_elem *next;// = elem->next;
  struct linked_list_elem *prev;
  struct linked_list_elem *tprev;
  if (autorelease && elem != ll->head) {
    __atomic_add_fetch(&elem->refcount, -1, __ATOMIC_SEQ_CST);
  }
  // Follow the prev pointer to the last live elment then step forewards one
  prev = elem;
  while (marked_as_deleted(tprev = prev->prev) && clear_mark(prev) != ll->head && clear_mark(tprev) != ll->head) {
    prev = clear_mark(tprev);
  }
  next = clear_mark(clear_mark(prev)->next);
  // printf("%p (tail)\n", ll->tail);
  // while (marked_as_deleted(next) && (tnext = clear_mark(next)) != ll->tail) {
  //   // printf("%p (next)\n%p (cleared next)\n", next, clear_mark(next));
  //   next = tnext->next;
  // }
  // printf("done\n");
  if (next != NULL && next != ll->tail) {
    __atomic_add_fetch(&next->refcount, 1, __ATOMIC_SEQ_CST);
  }
  return next;
}

/* inline */ struct linked_list_elem* get_prev(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease) {
  struct linked_list_elem *prev = elem->prev;
  struct linked_list_elem *telem;
  // struct linked_list_elem *next;
  // bool wasdel;
  //
  // while (!(wasdel = marked_as_deleted(elem->prev)) && (tprev = clear_mark(prev)->next) != elem && tprev != NULL && prev != NULL) {
  //   prev = tprev;
  // }
  // if (wasdel) {
  //   return get_prev(ll, get_next(ll, elem, true), autorelease);
  // }

  // if (elem == ll->head) {
  //   return elem;
  // }
  unsigned int ctr = 0;
  telem = elem;
  prev = telem->prev;
  while (marked_as_deleted(prev)) {
    ctr++;
    telem = clear_mark(prev);
    if (telem == ll->head) {
      prev = telem;
      break;
    }
    prev = telem->prev;
    if (ctr > 100000 && ctr % 10000 == 0) {
      printf("AAAAA %u\n", ctr);
    }
  }

  // int l1 = 0;
  // int l2 = 0;
  // next = elem;
  // // Walk back until prev is live
  // while (marked_as_deleted(tprev = next->prev) && (tprev = clear_mark(tprev)) != ll->head) {
  //   next = tprev;
  //   l1++;
  //   if (l1 > 1000000) {
  //     printf("Yikers\n");
  //   }
  // }
  // prev = tprev;
  // // Walk forewards until we hit the element or the first live element after it
  // while (
  //   (next = clear_mark(prev)->next) != (
  //     marked_as_deleted(elem->prev) ?
  //       get_next(ll, clone_ref(ll, elem), false) :
  //       elem
  //     ) && prev != NULL && next != NULL) {
  //   prev = next;
  //   l2++;
  //   if (l2 > 1000000) {
  //     printf("hmmm\n");
  //   }
  // }


  // if (marked_as_deleted(prev)) {
  //   while (marked_as_deleted(prev) && clear_mark(prev) != ll->head && prev != NULL) {
  //     prev = clear_mark(prev)->prev;
  //   }
  //   if (marked_as_deleted(prev)) {
  //     prev = clear_mark(prev);
  //   }
  // } else {
  //   // Walk prev forwards
  //   // The problem is that at any point we could be deleted and need to abort
  //   while (prev->next != elem) {
  //     prev = prev->next;
  //     if (marked_as_deleted(prev)) {
  //       return get_prev(ll, elem, autorelease);
  //     }
  //   }
  // }
  if (autorelease && elem != ll->tail && elem != ll->head) {
    __atomic_add_fetch(&elem->refcount, -1, __ATOMIC_SEQ_CST);
  }
  if (prev != NULL && prev != ll->head) {
    __atomic_add_fetch(&prev->refcount, 1, __ATOMIC_SEQ_CST);
  }
  return prev;
}

// int insert_after(struct linked_list_elem *prev, struct linked_list_elem *node) {
//   struct linked_list_elem *next = prev->next;
//   do {
//     while (prev->next != next) {
//       next = prev->next;
//     }
//     node->prev = prev;
//     node->next = next;
//   } while (!__atomic_compare_exchange(&prev->next, &next, &node, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
//   struct linked_list_elem *prev2;
//   struct linked_list_elem *last;
//   while (true) {
//     if (marked_as_deleted(next->prev) || node->next != next) {
//       break;
//     }
//     if (__atomic_compare_exchange(&next->prev, &next->prev, &node)) {
//       if (marked_as_deleted(node->prev)) {
//         prev2 = HelpInsert(node, next);
//         // HelpInsert transcription below:
//         last = NULL;
//         while (true) {
//           prev2 = prev->next;
//           if (prev2 == NULL) {
//             if (last != NULL) {
//               M
//             }
//           }
//         }
//       }
//       break;
//     }
//   }
// }
//
// int insert_head(struct linked_list *ll, const char *val) {
//   struct linked_list_elem *elem = malloc(sizeof(struct linked_list_elem));
//   struct linked_list_elem *next;
//   do {
//     elem->prev = NULL;
//     next = ll->head;
//     elem->next = next;
//   } while (!__atomic_compare_and_swap(&ll->head, &elem->next, elem, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
//   while (true) {
//     if (elem->next != next) {
//       elem = elem->next;
//       continue;
//     }
//     if (marked_as_deleted(next)) {
//       break; // Guess we don't need to fix it, it's deleted
//     }
//   }
//   // do {
//   //   elem->next = ll->head;
//   // } while (!__atomic_compare_and_swap(&ll->head, elem->next, elem, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
//   // __atomic_compare_and_swap(&elem->next->prev, &null_elem, )
// }
