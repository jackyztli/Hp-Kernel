/*
 *  kernel/thread.h
 *
 *  (C) 2021  Jacky
 */
#ifndef THREAD_H
#define THREAD_H

#include "stdint.h"
#include "kernel/memory.h"
#include "lib/list.h"

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
typedef uint32_t pid_t;

typedef struct {
    /* 中断号 */
    uint32_t vecNo;
    /* 通用寄存器 */
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    /* esp寄存器占位，实际不使用 */
    uint32_t espDummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    /* 段寄存器 */
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    /* cpu从低特权级进入到高特权级时使用 */
    uint32_t errCode;
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
} IntrStack;

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
    uint32_t *taskStack;
    /* 任务标识 */
    pid_t pid;
    /* 任务状态 */
    TaskStatus taskStatus;
    /* 任务名 */
    char name[16];
    /* 任务优先级 */
    uint8_t priority;
    /* 任务每次在处理器执行的时间 */
    uint8_t ticks;
    /* 此任务自上cpu运行的时间 */
    uint32_t elapsedTicks;
    /* 在一般链表中的节点，通常用于在运行链表中的阶段 */
    ListNode generalTag;
    /* 在所有线程队列中的节点 */
    ListNode threadListTag;
    /* 页表指针 */
    uint32_t *pgDir;
    /* 进程自己页表虚拟地址管理池 */
    VirtualMemPool progVaddrPool;
    /* 用户进程的虚拟地址 */
    MemBlockDesc memblockDesc[DESC_CNT];
    /* 任务魔数，用于判断边界 */
    uint32_t stackMagic;
} Task;


/* 线程创建函数 */
Task *Thread_Create(const char *name, uint32_t priority, ThreadFunc threadFunc, void *threadArgs);

/* 获取当前任务的PCB地址 */
Task *Thread_GetRunningTask(void);

/* 通过任务链表节点获取任务的PCB地址 */
Task *Thread_GetTaskPCB(const ListNode *listNode);

/* 任务调度 */
void Thread_Schedule(void);

/* 当前进程阻塞 */
void Thread_Block(TaskStatus status);

/* 当前任务被唤醒 */
void Thread_UnBlock(Task *task);

/* 任务初始化 */
void Thread_Init(void);

#endif