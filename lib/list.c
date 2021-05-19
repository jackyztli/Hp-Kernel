/*
 *  lib/list.c
 *
 *  (C) 2021  Jacky
 */

#include "list.h"
#include "stdint.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"

/* 链表初始化 */
void List_Init(List *list)
{
    list->head.prev = NULL;
    list->head.next = &(list->tail);
    list->tail.prev = &(list->head);
    list->tail.next = NULL;

    return;
}

/* 在头部插入一个节点 */
void List_Push(List *list, ListNode *listNode)
{
    ASSERT((list != NULL) && (listNode != NULL));

    /* 链表为全局的，需要关中断保护 */
    IntrStatus status = Idt_IntrDisable();

    list->head.next->prev = listNode;
    
    listNode->prev = &(list->head);
    listNode->next = list->head.next;
    
    list->head.prev = NULL;
    list->head.next = listNode;

    /* 处理完成后需要打开中断 */
    Idt_SetIntrStatus(status);

    return;
}

/* 在尾部插入一个节点 */
void List_Append(List *list, ListNode *listNode)
{
    ASSERT((list != NULL) && (listNode != NULL));

    /* 链表为全局的，需要关中断保护 */
    IntrStatus status = Idt_IntrDisable();

    list->tail.prev->next = listNode;

    listNode->prev = list->tail.prev;
    listNode->next = &(list->tail);
    
    list->tail.prev = listNode;
    list->tail.next = NULL;

    /* 处理完成后需要打开中断 */
    Idt_SetIntrStatus(status);

    return;
}

/* 在链表中删除一个节点 */
void List_Remove(ListNode *listNode)
{
    /* 链表为全局的，需要关中断保护 */
    IntrStatus status = Idt_IntrDisable();

    listNode->prev->next = listNode->next;
    listNode->next->prev = listNode->prev;
    
    /* 处理完成后需要打开中断 */
    Idt_SetIntrStatus(status);

    return;
}

/* 判断是否为空链表 */
bool List_IsEmpty(List *list)
{
    ASSERT(list != NULL);

    if (list->head.next == &(list->tail)) {
        return true;
    }

    return false;
}

/* 从链表头弹出一个节点 */
ListNode *List_Pop(List *list)
{
    ASSERT(List_IsEmpty(list) != true);

    /* 链表为全局的，需要关中断保护 */
     IntrStatus status = Idt_IntrDisable();
    ListNode *listNode = list->head.next;

    listNode->prev->next = listNode->next;
    listNode->next->prev = listNode->prev;

    /* 处理完成后需要打开中断 */
    Idt_SetIntrStatus(status);

    return listNode;
}

/* 在链表中查找节点 */
bool List_Find(const List *list, const ListNode *listNode)
{
    ASSERT((list != NULL) && (listNode != NULL));

    const ListNode *listNodeTmp = list->head.next;

    while (listNodeTmp != &(list->tail)) {
        if (listNodeTmp == listNode) {
            return true;
        }
        listNodeTmp = listNodeTmp->next;
    }

    return false;
}

/* 遍历链表节点，对链表节点执行func(node)操作 */
void List_Traversal(List *list, Func func, void *arg)
{
    ListNode *listNode = list->head.next;
    while (listNode != &(list->tail)) {
        func(listNode, arg);
        listNode = listNode->next;
    }

    return;
}