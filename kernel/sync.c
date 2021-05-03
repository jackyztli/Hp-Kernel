/*
 *  kernel/sync.c
 *
 *  (C) 2021  Jacky
 */

#include "sync.h"
#include "stdint.h"
#include "kernel/panic.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "lib/list.h"

/* 对锁进行P操作 */
void Lock_P(Lock *lock)
{
    /* 关中断，对锁的操作需要保证原子性 */
    IntrStatus status = Idt_IntrDisable();
    while (lock->value == 0) {
        ASSERT(List_Find(&lock->waiters, &(Thread_GetRunningTask()->generalTag)) == false);
        /* 将当前任务加入到阻塞队列中 */
        List_Append(&lock->waiters, &(Thread_GetRunningTask()->generalTag));
        /* 获取锁失败，当前任务阻塞 */
        Thread_Block(TASK_BLOCKED);
    }

    /* 已经获取到锁资源，任务被重新唤起 */
    lock->value--;
    ASSERT(lock->value == 0);

    Idt_SetIntrStatus(status);

    return;
}

/* 对锁进行V操作 */
void Lock_V(Lock *lock)
{
    /* 关中断，对锁的操作需要保证原子性 */
    IntrStatus status = Idt_IntrDisable();
    ASSERT(lock->value == 0);
    if (List_IsEmpty(&lock->waiters) != true) {
        /* 从阻塞队列中获取链表头节点，将其解阻塞 */
        ListNode *node = List_Pop(&lock->waiters);
        /* 获取节点对应任务的PCB */
        Task *task = Thread_GetTaskPCB(node);
        Thread_UnBlock(task);
    }

    /* 已经获取到锁资源，任务被重新唤起 */
    lock->value++;
    ASSERT(lock->value == 1);
    Idt_SetIntrStatus(status);

    return;
}

/* 获取锁操作 */
void Lock_Lock(Lock *lock)
{
    if (lock->holder != Thread_GetRunningTask()) {
        /* 尝试对锁进行P操作 */
        Lock_P(lock);
        /* 已经获取到锁 */
        lock->holder = Thread_GetRunningTask();
        lock->holderRepeatNum++;
        ASSERT(lock->holderRepeatNum == 1);
    } else {
        /* 锁的持有者重复获取 */
        lock->holderRepeatNum++;
    }

    return;
}

/* 释放锁操作 */
void Lock_UnLock(Lock *lock)
{
    /* 只有持有锁的任务才能释放锁 */
    ASSERT(lock->holder == Thread_GetRunningTask());

    if (lock->holderRepeatNum > 1) {
        lock->holderRepeatNum--;
        return;
    }

    lock->holder = NULL;
    lock->holderRepeatNum--;
    Lock_V(lock);

    return;
}   

/* 初始化锁 */
void Lock_Init(Lock *lock)
{
    /* 二元锁 */
    lock->value = 1;
    lock->holder = NULL;
    lock->holderRepeatNum = 0;
    List_Init(&lock->waiters);

    return;
}