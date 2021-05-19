/*
 *  lib/list.h
 *
 *  (C) 2021  Jacky
 */
#ifndef LIST_H
#define LIST_H

#include "stdint.h"

typedef struct _ListNode {
    struct _ListNode *prev;
    struct _ListNode *next;
} ListNode;

typedef struct {
    struct _ListNode head;
    struct _ListNode tail;
} List;

/* 结构体成员的偏移值 */
#define OFFSET(struct_name, member) (int32_t)(&((struct_name *)0)->member)
/* 通过链接节点获取对应的结构体空间 */
#define ELEM2ENTRY(struct_name, member, elem_ptr) \
    (struct_name *)((uintptr_t)elem_ptr - OFFSET(struct_name, member))

typedef void (*Func) (ListNode *, void *);

/* 链表初始化 */
void List_Init(List *list);
/* 在头部插入一个节点 */
void List_Push(List *list, ListNode *listNode);
/* 在尾部插入一个节点 */
void List_Append(List *list, ListNode *listNode);
/* 在链表中删除一个节点 */
void List_Remove(ListNode *listNode);
/* 判断是否为空链表 */
bool List_IsEmpty(List *list);
/* 从链表头弹出一个节点 */
ListNode *List_Pop(List *list);
/* 在链表中查找节点 */
bool List_Find(const List *list, const ListNode *listNode);
/* 遍历链表节点，对链表节点执行func(node)操作 */
void List_Traversal(List *list, Func func, void *arg);

#endif