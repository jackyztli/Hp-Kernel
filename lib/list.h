/*
 *  lib/list.h
 *
 *  (C) 2021  Jacky
 */
#ifndef LIST_H
#define LIST_H

#include "stdint.h"

typedef struct {
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct {
    ListNode head;
    ListNode tail;
} List;

/* 链表初始化 */
void List_Init(List *list);
/* 在尾部插入一个节点 */
void List_Append(List *list, ListNode *listNode);

#endif