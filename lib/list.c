#include <list.h>
#include <stdint.h>

void add_list_head(struct list *l, struct list *n)
{
    //panic((l != NULL) && (n != NULL));
    n->next = l;
    l = n;
}

void add_list_tail(struct list *l, struct list *n)
{
    if (l == NULL) {
        l = n;
        return;
    }

    while (l->next) {
        l = l->next;
    }

    l->next = n;
}