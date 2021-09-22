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

bool list_empty(const struct list *l)
{
    return (l == NULL) ? true : false;
}

/* 从列表头部取出一个节点 */
struct list *list_pop(struct list *l)
{
    if (list_empty(l)) {
        return NULL;
    }

    struct list *node = l;
    l = l->next;
    node->next = NULL;
    return node;
}