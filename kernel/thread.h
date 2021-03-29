/*
 *  kernel/thread.h
 *
 *  (C) 2021  Jacky
 */
#ifndef THREAD_H
#define THREAD_H

#include "stdint.h"

/* 进程或线程状态枚举 */
typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANDING,
    TASK_DIED
} TaskStatus;

/* 任务通用函数定义 */
typedef void (*ThreadFunc) (void *threadArgs);

typedef struct {
    /* 以下用于保存函数调用过程中的寄存器 */
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    /* 在函数调用中，eip保存着调用者的返回地址，处理器根据CS:EIP找到对应地址运行 
     * 此处定义eip是为了让线程在ret返回时调用该地址，其实是调用用户注册的线程处理地址
    */
    void (*eip) (ThreadFunc threadFunc, void *threadArgs);
    /* 占4字节空间，充当返回地址，实际没使用 */
    void *unusedRetAddr;
    ThreadFunc threadFunc;
    void *threadArgs;
} TaskStack;

typedef struct {
    /* 任务私有栈 */
    TaskStack *taskStack;
    /* 任务状态 */
    TaskStatus taskStatus;
    /* 任务优先级 */
    uint32_t priority;
    /* 任务名 */
    char name[16];
    /* 任务魔数，用于判断边界 */
    uint32_t stackMagic;
} Task;


/* 线程创建函数 */
Task *Thread_Create(const char *name, uint32_t priority, ThreadFunc threadFunc, void *threadArgs);

#endif