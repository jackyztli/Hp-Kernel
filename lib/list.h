#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>

struct list {
    struct list *next;
};

#define offset(struct_type, member) (intptr_t)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
	 (struct_type *)((intptr_t)elem_ptr - offset(struct_type, struct_member_name))

void add_list_head(struct list *l, struct list *n);
void add_list_tail(struct list *l, struct list *n);
bool list_empty(const struct list *l);
/* 从列表头部取出一个节点 */
struct list *list_pop(struct list *l);

#endif