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

/* 链表初始化 */
void List_Init(List *list);
/* 在尾部插入一个节点 */
void List_Append(List *list, ListNode *listNode);
/* 判断是否为空链表 */
bool List_IsEmpty(List *list);
/* 从链表头弹出一个节点 */
ListNode *List_Pop(List *list);
/* 在链表中查找节点 */
bool List_Find(const List *list, const ListNode *listNode);

#endif