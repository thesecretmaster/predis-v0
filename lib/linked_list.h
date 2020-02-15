#include <stdbool.h>

struct llval {
  int thread_num;
  int ctr;
  bool deleteme;
};

struct linked_list_elem;

struct linked_list;

/* __inline__ */ int get_length(struct linked_list *ll);
/* __inline__ */ struct llval *get_value(struct linked_list_elem*);
/* __inline__ */ struct linked_list_elem* get_tail(struct linked_list *ll);
/* __inline__ */ struct linked_list_elem* get_head(struct linked_list *ll);
/* __inline__ */ struct linked_list_elem* get_next(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease);
/* __inline__ */ struct linked_list_elem* get_prev(struct linked_list *ll, struct linked_list_elem *elem, bool autorelease);
/* __inline__ */ void release(struct linked_list *ll, struct linked_list_elem *elem);
/* __inline__ */ struct linked_list_elem* clone_ref(struct linked_list *ll, struct linked_list_elem *elem);

int insert_after(struct linked_list *ll, struct linked_list_elem *prev, struct llval value);
int delete_elem(struct linked_list *ll, struct linked_list_elem *elem);
int clean(struct linked_list *ll);
struct linked_list *initialize_ll(void);
