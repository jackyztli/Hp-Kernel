/*
 *  kernel/sync.h
 *
 *  (C) 2021  Jacky
 */
#ifndef SYNC_H
#define SYNC_H

#include "stdint.h"
#include "kernel/thread.h"
#include "lib/list.h"

typedef struct {
    /* 锁的信号量 */
    uint8_t value;
    /* 锁的持有者 */
    Task *holder;
    /* 锁的持有者重复申请次数 */
    uint32_t holderRepeatNum;
    /* 信号量等待队列 */
    List waiters;
} Lock;

/* 对锁进行P操作 */
void Lock_P(Lock *lock);

/* 对锁进行V操作 */
void Lock_V(Lock *lock);

/* 获取锁操作 */
void Lock_Lock(Lock *lock);

/* 释放锁操作 */
void Lock_UnLock(Lock *lock);

/* 初始化锁 */
void Lock_Init(Lock *lock);

#endif