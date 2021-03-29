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

/* 在尾部插入一个节点 */
void List_Append(List *list, ListNode *listNode)
{
    ASSERT((list != NULL) && (listNode != NULL));

    /* 链表为全局的，需要关中断保护 */
    Idt_IntrDisable();

    list->tail.prev->next = listNode;

    listNode->prev = list->tail.prev;
    listNode->next = &(list->tail);
    
    list->tail.prev = listNode;
    list->tail.next = NULL;

    /* 处理完成后需要打开中断 */
    Idt_IntrEnable();

    return;
}