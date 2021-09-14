#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>

struct list {
    struct list *next;
};

void add_list_head(struct list *l, struct list *n);
void add_list_tail(struct list *l, struct list *n);

#endif