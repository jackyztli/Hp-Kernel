/*
 *  kernel/thread.h
 *
 *  (C) 2021  Jacky
 */

#include "thread.h"
#include "stdint.h"
#include "kernel/memory.h"
#include "kernel/panic.h"
#include "lib/string.h"

static void Thread_KernelStart(ThreadFunc threadFunc, void *threadArgs)
{
    /* 函数入参允许为NULL */
    ASSERT(threadFunc != NULL);

    threadFunc(threadArgs);
}

/* 任务栈初始化 */
static inline void Thread_TaskInit(TaskStack *taskStack, ThreadFunc threadFunc, void *threadArgs)
{
    /* 待保存寄存器初始化为0 */
    taskStack->ebp = 0;
    taskStack->ebx = 0;
    taskStack->edi = 0;
    taskStack->esi = 0;

    taskStack->eip = Thread_KernelStart;
    taskStack->threadFunc = threadFunc;
    taskStack->threadArgs = threadArgs;
}

/* 线程创建函数 */
Task *Thread_Create(const char *name, uint32_t priority, ThreadFunc threadFunc, void *threadArgs)
{
    /* 创建线程PCB空间，即1个页表 */
    Task *task = Mem_GetKernelPages(1);

    /* 任务栈空间存储在PCB高地址处 */
    task->taskStack = (TaskStack *)((uintptr_t)task + PAGE_SIZE - sizeof(TaskStack));
    Thread_TaskInit(task->taskStack, threadFunc, threadArgs);
    task->taskStatus = TASK_RUNNING;
    task->priority = priority;
    strcpy(task->name, name);
    task->stackMagic = 0x19AE1617;

    /* 通过ret的方式调用用户提供的函数 */
    __asm__ volatile("movl %0, %%esp; \
                      pop %%ebp;      \
                      pop %%ebx;      \
                      pop %%edi;      \
                      pop %%esi;      \
                      ret" : : "g"(task->taskStack) : "memory");

    return task;
}